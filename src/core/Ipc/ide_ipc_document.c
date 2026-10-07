#include "core/Ipc/ide_ipc_document.h"
#include "core/Ipc/ide_ipc_server.h"
#include "core/Ipc/ide_ipc_workspace.h"
#include <pthread.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
static IdeDocumentHandler handler;
static bool inflight, pending, executing, completed, abandoned, success;
static json_object *request_args, *response;
static char failure[256], root[1024];
static uint64_t generation;
void ide_ipc_document_set_handler(IdeDocumentHandler h) {
    pthread_mutex_lock(&lock); handler=h; pthread_mutex_unlock(&lock);
}
bool ide_ipc_document_request(json_object* args, json_object** result, char* error, size_t cap) {
    *result=NULL;
    pthread_mutex_lock(&lock);
    if (!handler || inflight) {
        snprintf(error,cap,"document_control_busy_or_unavailable");
        pthread_mutex_unlock(&lock); return false;
    }
    request_args=json_object_get(args);
    ide_ipc_copy_project_root(root,sizeof(root)); generation=ide_ipc_workspace_generation();
    inflight=pending=true; executing=completed=abandoned=success=false;
    struct timespec end; clock_gettime(CLOCK_REALTIME,&end); end.tv_sec+=10;
    while (!completed) {
        if (pthread_cond_timedwait(&cond,&lock,&end)==ETIMEDOUT) {
            pending=false; abandoned=true;
            if (!executing) { json_object_put(request_args); request_args=NULL; inflight=false; }
            snprintf(error,cap,"document_control_timeout_inspect_before_retry");
            pthread_mutex_unlock(&lock); return false;
        }
    }
    bool ok=success; if (ok) { *result=response; response=NULL; }
    else snprintf(error,cap,"%s",failure);
    inflight=false; pthread_mutex_unlock(&lock); return ok;
}
void ide_ipc_document_pump(void) {
    pthread_mutex_lock(&lock);
    if (!pending || !inflight) { pthread_mutex_unlock(&lock); return; }
    pending=false; executing=true;
    json_object* args=request_args; IdeDocumentHandler h=handler;
    uint64_t expected=generation; char expected_root[1024];
    snprintf(expected_root,sizeof(expected_root),"%s",root);
    pthread_mutex_unlock(&lock);
    char current[1024],error[256]={0}; json_object* result=NULL;
    ide_ipc_copy_project_root(current,sizeof(current));
    bool ok=expected==ide_ipc_workspace_generation() && !strcmp(current,expected_root);
    if (!ok) snprintf(error,sizeof(error),"stale_workspace_binding");
    else ok=h && h(args,&result,error,sizeof(error));
    json_object_put(args);
    pthread_mutex_lock(&lock);
    request_args=NULL; executing=false; completed=true; success=ok;
    if (!ok || abandoned) { if (result) json_object_put(result); response=NULL; }
    else response=result;
    snprintf(failure,sizeof(failure),"%s",error);
    if (abandoned) inflight=false;
    pthread_cond_broadcast(&cond); pthread_mutex_unlock(&lock);
}
