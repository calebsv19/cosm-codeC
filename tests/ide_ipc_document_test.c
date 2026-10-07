#include "core/Ipc/ide_ipc_document.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
static uint64_t generation=1;
void ide_ipc_copy_project_root(char* out,size_t cap) { snprintf(out,cap,"/lesson"); }
uint64_t ide_ipc_workspace_generation(void) { return generation; }
static bool apply(json_object* args,json_object** out,char* error,size_t cap) {
 (void)args;(void)error;(void)cap;*out=json_object_new_object();
 json_object_object_add(*out,"inspected",json_object_new_boolean(true));return true;
}
typedef struct { json_object* args;json_object* result;bool ok;char error[256]; } Request;
static void* run(void* ptr) { Request* r=ptr;r->ok=ide_ipc_document_request(r->args,&r->result,r->error,sizeof(r->error));return NULL; }
int main(void) {
 ide_ipc_document_set_handler(apply);
 Request r={.args=json_object_new_object()};pthread_t thread;
 assert(!pthread_create(&thread,NULL,run,&r));
 for(int i=0;i<100;++i) {ide_ipc_document_pump();usleep(1000);}
 assert(!pthread_join(thread,NULL));assert(r.ok && r.result);
 json_object_put(r.result);json_object_put(r.args);
 puts("document main-thread dispatch passed");return 0;
}
