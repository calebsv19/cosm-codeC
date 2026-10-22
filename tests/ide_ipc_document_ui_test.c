#include "test_tmp_root.h"
#include "core/Ipc/ide_ipc_document.h"
#include "core/Ipc/ide_ipc_edit_apply.h"
#include "core/Analysis/analysis_scheduler.h"
#include "app/GlobalInfo/core_state.h"
#include "ide/Panes/Editor/editor_view.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static IDECoreState core;
static char root[1024];
IDECoreState* getCoreState(void) { return &core; }
void ide_ipc_copy_project_root(char* out,size_t cap) { snprintf(out,cap,"%s",root); }
bool isSaveQueueBusy(void) { return false; }
void analysis_scheduler_request_file(const char* path,uint32_t reasons,bool force) { (void)path;(void)reasons;(void)force; }
char* getBufferSnapshot(EditorBuffer* buffer,size_t* length) {
 *length=strlen(buffer->lines[0]);return strdup(buffer->lines[0]);
}
bool ide_ipc_compute_file_hash_hex(const char* path,char* out,size_t cap) {
 FILE* f=fopen(path,"rb");if (!f) return false;
 unsigned long long hash=1469598103934665603ULL;int ch;
 while ((ch=fgetc(f))!=EOF) {hash^=(unsigned char)ch;hash*=1099511628211ULL;}
 bool ok=!ferror(f);fclose(f);snprintf(out,cap,"%016llx",hash);return ok;
}
int main(void) {
 snprintf(root,sizeof(root),"" IDE_TEST_TMPDIR "/ide-document-ui-XXXXXX");assert(mkdtemp(root));
 char path[1100];snprintf(path,sizeof(path),"%s/lesson.c",root);
 FILE* f=fopen(path,"w");assert(f);fputs("old\n",f);fclose(f);
 char* line="new\n";EditorBuffer buffer={.lines=&line,.lineCount=1};
 OpenFile file={.filePath=path,.buffer=&buffer,.isModified=true};
 OpenFile* files[]={&file};
 EditorView view={.type=VIEW_LEAF,.fileCount=1,.openFiles=files};core.persistentEditorView=&view;
 json_object* args=json_object_new_object();json_object* out=NULL;char error[256];
 json_object_object_add(args,"path",json_object_new_string("lesson.c"));
 json_object_object_add(args,"op",json_object_new_string("inspect"));
 assert(ide_ipc_document_from_ui(args,&out,error,sizeof(error)));
 json_object *b,*d;assert(json_object_object_get_ex(out,"buffer_fnv64",&b));assert(json_object_object_get_ex(out,"disk_fnv64",&d));
 char bh[17],dh[17];snprintf(bh,sizeof(bh),"%s",json_object_get_string(b));snprintf(dh,sizeof(dh),"%s",json_object_get_string(d));
 json_object_put(out);out=NULL;
 json_object_object_add(args,"op",json_object_new_string("save"));
 json_object_object_add(args,"expected_buffer_fnv64",json_object_new_string("0000000000000000"));
 json_object_object_add(args,"expected_disk_fnv64",json_object_new_string(dh));
 assert(!ide_ipc_document_from_ui(args,&out,error,sizeof(error)));assert(file.isModified);
 json_object_object_add(args,"expected_buffer_fnv64",json_object_new_string(bh));
 f=fopen(path,"w");fputs("external\n",f);fclose(f);
 assert(!ide_ipc_document_from_ui(args,&out,error,sizeof(error)));assert(file.isModified);
 f=fopen(path,"w");fputs("old\n",f);fclose(f);
 assert(ide_ipc_document_from_ui(args,&out,error,sizeof(error)));assert(!file.isModified);
 char actual[17];assert(ide_ipc_compute_file_hash_hex(path,actual,sizeof(actual)));assert(!strcmp(actual,bh));
 json_object_put(out);json_object_put(args);unlink(path);rmdir(root);
 puts("live-buffer inspect, changed-buffer/disk refusal and saved readback passed");return 0;
}
