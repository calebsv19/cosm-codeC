#include "core/Ipc/ide_ipc_document.h"
#include "core/Ipc/ide_ipc_server.h"
#include "core/Ipc/ide_ipc_path_guard.h"
#include "core/Ipc/ide_ipc_edit_apply.h"
#include "core/CommandBus/save_queue.h"
#include "core/Analysis/analysis_scheduler.h"
#include "app/GlobalInfo/core_state.h"
#include "ide/Panes/Editor/editor_view.h"
#include "ide/Panes/Editor/editor_buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

static void find_file(EditorView* view,const char* path,OpenFile** found,bool* conflict) {
    if (!view) return;
    if (view->type==VIEW_SPLIT) {
        find_file(view->childA,path,found,conflict); find_file(view->childB,path,found,conflict); return;
    }
    for (int i=0;i<view->fileCount;++i) {
        OpenFile* file=view->openFiles[i];
        if (!file || !file->filePath || strcmp(file->filePath,path)) continue;
        if (*found && *found!=file) *conflict=true;
        else *found=file;
    }
}
static const char* string_arg(json_object* args,const char* key) {
    json_object* value=NULL;
    return json_object_object_get_ex(args,key,&value) && json_object_is_type(value,json_type_string)
        ? json_object_get_string(value) : NULL;
}
static void hash_text(const char* text,size_t length,char* out) {
    unsigned long long hash=1469598103934665603ULL;
    for (size_t i=0;i<length;++i) { hash^=(unsigned char)text[i]; hash*=1099511628211ULL; }
    snprintf(out,17,"%016llx",hash);
}
bool ide_ipc_document_from_ui(json_object* args,json_object** result,char* error,size_t cap) {
    const char* path=string_arg(args,"path"); const char* op=string_arg(args,"op");
    char root[1024],absolute[4096],disk_hash[17],buffer_hash[17];
    *result=NULL; ide_ipc_copy_project_root(root,sizeof(root));
    if (!path || !op || (strcmp(op,"inspect") && strcmp(op,"save"))) {
        snprintf(error,cap,"document_requires_path_and_inspect_or_save"); return false;
    }
    if (!ide_ipc_resolve_workspace_existing_path(root,path,absolute,sizeof(absolute),error,cap)) return false;
    if (isSaveQueueBusy()) { snprintf(error,cap,"document_save_queue_busy"); return false; }
    IDECoreState* core=getCoreState(); OpenFile* file=NULL; bool conflict=false;
    find_file(core?core->persistentEditorView:NULL,absolute,&file,&conflict);
    if (conflict) { snprintf(error,cap,"document_multiple_independent_buffers"); return false; }
    if (!ide_ipc_compute_file_hash_hex(absolute,disk_hash,sizeof(disk_hash))) {
        snprintf(error,cap,"document_disk_read_failed"); return false;
    }
    size_t length=0; char* text=file && file->buffer ? getBufferSnapshot(file->buffer,&length) : NULL;
    if (file && (!text || length>32768)) {
        free(text); snprintf(error,cap,"document_buffer_unavailable_or_too_large"); return false;
    }
    if (text) hash_text(text,length,buffer_hash); else buffer_hash[0]=0;
    if (!strcmp(op,"save")) {
        const char* expected_buffer=string_arg(args,"expected_buffer_fnv64");
        const char* expected_disk=string_arg(args,"expected_disk_fnv64");
        if (!file || !expected_buffer || !expected_disk || strcmp(expected_buffer,buffer_hash) ||
            strcmp(expected_disk,disk_hash)) {
            free(text); snprintf(error,cap,"document_save_conflict_inspect_again"); return false;
        }
        struct stat st;
        if (stat(absolute,&st) || !S_ISREG(st.st_mode) || st.st_nlink!=1) {
            free(text); snprintf(error,cap,"document_save_requires_regular_single_link_file"); return false;
        }
        char temporary[4112]; snprintf(temporary,sizeof(temporary),"%s.mcp-XXXXXX",absolute);
        int fd=mkstemp(temporary);
        if (fd<0) { free(text); snprintf(error,cap,"document_save_create_failed"); return false; }
        bool ok=fchmod(fd,st.st_mode & 0777)==0; size_t done=0;
        while (ok && done<length) {
            ssize_t n=write(fd,text+done,length-done);
            if (n<=0) ok=false; else done+=(size_t)n;
        }
        if (ok && fsync(fd)) ok=false;
        if (close(fd)) ok=false;
        char latest[17]; struct stat current;
        if (ok && (stat(absolute,&current) || current.st_ino!=st.st_ino ||
            !ide_ipc_compute_file_hash_hex(absolute,latest,sizeof(latest)) || strcmp(latest,expected_disk))) ok=false;
        if (ok && rename(temporary,absolute)) ok=false;
        if (!ok) { unlink(temporary); free(text); snprintf(error,cap,"document_save_failed_buffer_preserved"); return false; }
        file->isModified=false; file->showSavedTag=true; file->savedTagTimestamp=SDL_GetTicks();
        analysis_scheduler_request_file(absolute,ANALYSIS_REASON_EDITOR_SAVE,false);
        snprintf(disk_hash,sizeof(disk_hash),"%s",buffer_hash);
    }
    json_object* out=json_object_new_object();
    json_object_object_add(out,"path",json_object_new_string(path));
    json_object_object_add(out,"open",json_object_new_boolean(file!=NULL));
    json_object_object_add(out,"modified",json_object_new_boolean(file && file->isModified));
    json_object_object_add(out,"disk_fnv64",json_object_new_string(disk_hash));
    json_object_object_add(out,"buffer_fnv64",text?json_object_new_string(buffer_hash):NULL);
    json_object_object_add(out,"buffer_text",text?json_object_new_string_len(text,(int)length):NULL);
    json_object_object_add(out,"saved",json_object_new_boolean(!strcmp(op,"save")));
    free(text); *result=out; return true;
}
