#include "core/Ipc/ide_ipc_workspace.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
static char root[1024]="/old";
void ide_ipc_copy_project_root(char* out,size_t cap) { snprintf(out,cap,"%s",root); }
static bool apply(const char* path,char* error,size_t cap) {
 if (!strcmp(path,"/dirty")) { snprintf(error,cap,"workspace_has_unsaved_documents");return false; }
 snprintf(root,sizeof(root),"%s",path);ide_ipc_workspace_changed();return true;
}
typedef struct { const char* path; const char* root; uint64_t generation; bool ok; char error[256]; } Request;
static void* run(void* ptr) { Request* r=ptr;r->ok=ide_ipc_workspace_request(r->path,r->root,r->generation,r->error,sizeof(r->error));return NULL; }
static void test(Request* r) {
 pthread_t thread;assert(!pthread_create(&thread,NULL,run,r));
 for(int i=0;i<50;++i) {ide_ipc_workspace_pump();usleep(1000);}
 assert(!pthread_join(thread,NULL));
}
int main(void) {
 char error[256];assert(!ide_ipc_workspace_request("/new","/old",1,error,sizeof(error)));
 ide_ipc_workspace_set_handler(apply);
 Request r={"/new","/old",1,false,{0}};test(&r);assert(r.ok);assert(!strcmp(root,"/new"));assert(ide_ipc_workspace_generation()==2);
 Request stale={"/wrong","/old",1,false,{0}};test(&stale);assert(!stale.ok);assert(strstr(stale.error,"stale"));assert(!strcmp(root,"/new"));
 Request dirty={"/dirty","/new",2,false,{0}};test(&dirty);assert(!dirty.ok);assert(strstr(dirty.error,"unsaved"));assert(ide_ipc_workspace_generation()==2);
 Request next={"/final","/new",2,false,{0}};test(&next);assert(next.ok);assert(!strcmp(root,"/final"));
 puts("workspace apply, stale binding, refused switch and continuation passed");return 0;
}
