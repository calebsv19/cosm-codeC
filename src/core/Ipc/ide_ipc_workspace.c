#include "core/Ipc/ide_ipc_workspace.h"
#include "core/Ipc/ide_ipc_server.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
static IdeWorkspaceHandler handler;
static bool inflight, pending, completed, success, executing, abandoned;
static uint64_t generation = 1, expected_generation, ready_generation = 1;
static char target[1024], expected_root[1024], failure[256];
void ide_ipc_workspace_set_handler(IdeWorkspaceHandler h) {
    pthread_mutex_lock(&lock); handler=h; pthread_mutex_unlock(&lock);
}
uint64_t ide_ipc_workspace_generation(void) {
    pthread_mutex_lock(&lock); uint64_t g=generation; pthread_mutex_unlock(&lock); return g;
}
void ide_ipc_workspace_changed(void) {
    pthread_mutex_lock(&lock); ++generation; pthread_mutex_unlock(&lock);
}
bool ide_ipc_workspace_request(const char* path, const char* root, uint64_t expected,
                               char* error, size_t cap) {
    if (!path || strlen(path)>=sizeof(target) || !root || strlen(root)>=sizeof(expected_root)) {
        snprintf(error,cap,"invalid_workspace_request"); return false;
    }
    pthread_mutex_lock(&lock);
    if (!handler || inflight) {
        snprintf(error,cap,"workspace_control_busy_or_unavailable"); pthread_mutex_unlock(&lock); return false;
    }
    snprintf(target,sizeof(target),"%s",path); snprintf(expected_root,sizeof(expected_root),"%s",root);
    expected_generation=expected; inflight=pending=true; executing=completed=success=abandoned=false; failure[0]=0;
    struct timespec end; clock_gettime(CLOCK_REALTIME,&end); end.tv_sec+=10;
    while (!completed) {
        if (pthread_cond_timedwait(&cond,&lock,&end)==ETIMEDOUT) {
            pending=false; abandoned=true;
            /* An executing UI transaction must retain its slot until completion. */
            if (!executing) inflight=false;
            snprintf(error,cap,"workspace_switch_timeout_inspect_current_binding");
            pthread_mutex_unlock(&lock); return false;
        }
    }
    bool ok=success; if (!ok) snprintf(error,cap,"%s",failure);
    inflight=false; pthread_mutex_unlock(&lock); return ok;
}
void ide_ipc_workspace_pump(void) {
    char path[1024],root[1024]; uint64_t expected; IdeWorkspaceHandler h;
    pthread_mutex_lock(&lock);
    if (!pending || !inflight) { pthread_mutex_unlock(&lock); return; }
    pending=false; executing=true; h=handler; expected=expected_generation;
    snprintf(path,sizeof(path),"%s",target); snprintf(root,sizeof(root),"%s",expected_root);
    pthread_mutex_unlock(&lock);
    char error[256]={0}; char current[1024];
    ide_ipc_copy_project_root(current,sizeof(current));
    bool ok=expected==ide_ipc_workspace_generation() && !strcmp(root,current);
    if (!ok) snprintf(error,sizeof(error),"stale_workspace_binding");
    else ok=h && h(path,error,sizeof(error));
    pthread_mutex_lock(&lock); executing=false; success=ok; completed=true; if (abandoned) inflight=false;
    snprintf(failure,sizeof(failure),"%s",error); pthread_cond_broadcast(&cond); pthread_mutex_unlock(&lock);
}

void ide_ipc_workspace_mark_ready(void) { pthread_mutex_lock(&lock); ready_generation=generation; pthread_mutex_unlock(&lock); }
bool ide_ipc_workspace_ready(void) { pthread_mutex_lock(&lock); bool ready=ready_generation==generation; pthread_mutex_unlock(&lock);return ready; }
