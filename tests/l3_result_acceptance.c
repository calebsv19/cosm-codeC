#include <assert.h>
#include "ide/Panes/Editor/editor_view_file_helpers.h"
#include EVENT_LOOP_SOURCE
static uint64_t applied(void){CompletedResultsQueueStats s;completed_results_queue_snapshot(&s);return s.results_applied;}
int main(void){
 SDL_Init(SDL_INIT_TIMER);mainthread_context_set_owner_current();initCoreState();completed_results_queue_init();loop_events_init();analysis_status_init();
 strcpy(projectPath,"/owned");
 FrameContext ctx={0};OpenFile file={0};file.filePath="/owned/a.c";file.documentRevision=editor_next_document_revision(0);
 OpenFile* files[]={&file};EditorView view={0};view.type=VIEW_LEAF;view.openFiles=files;view.fileCount=1;getCoreState()->activeEditorView=&view;
 CompletedResult r={0};r.kind=COMPLETED_RESULT_ANALYSIS_PROGRESS;r.subsystem=COMPLETED_SUBSYSTEM_ANALYSIS;r.workspace_generation=ide_ipc_workspace_generation();r.payload.analysis_progress.analysis_run_id=10;strcpy(r.payload.analysis_progress.project_root,projectPath);r.has_document_revision=true;strcpy(r.document_path,file.filePath);r.document_revision=file.documentRevision;
 apply_completed_result(&ctx,&r);assert(applied()==1);
 file.documentRevision=editor_next_document_revision(file.documentRevision);apply_completed_result(&ctx,&r);assert(applied()==1); /* edit */
 r.document_revision=file.documentRevision;view.fileCount=0;apply_completed_result(&ctx,&r);assert(applied()==1); /* close */
 view.fileCount=1;file.documentRevision=editor_next_document_revision(0);apply_completed_result(&ctx,&r);assert(applied()==1); /* same path/slot reopen */
 r.document_revision=file.documentRevision;r.payload.analysis_progress.analysis_run_id=12;apply_completed_result(&ctx,&r);assert(applied()==2);
 r.payload.analysis_progress.analysis_run_id=11;apply_completed_result(&ctx,&r);assert(applied()==2); /* out of order */
 r.payload.analysis_progress.analysis_run_id=13;ide_ipc_workspace_changed();apply_completed_result(&ctx,&r);assert(applied()==2); /* same root new lifetime */
 r.workspace_generation=ide_ipc_workspace_generation();strcpy(r.payload.analysis_progress.project_root,"/other");apply_completed_result(&ctx,&r);assert(applied()==2);
 strcpy(r.payload.analysis_progress.project_root,projectPath);apply_completed_result(&ctx,&r);assert(applied()==3);
 memset(&r,0,sizeof(r));r.kind=COMPLETED_RESULT_ANALYSIS_FINISHED;r.subsystem=COMPLETED_SUBSYSTEM_ANALYSIS;r.workspace_generation=ide_ipc_workspace_generation();r.payload.analysis_finished.analysis_run_id=14;r.payload.analysis_finished.had_error=true;r.payload.analysis_finished.library_index_stamp=library_index_combined_stamp();strcpy(r.payload.analysis_finished.project_root,projectPath);
 apply_completed_result(&ctx,&r);assert(applied()==4);apply_completed_result(&ctx,&r);assert(applied()==4); /* duplicate terminal */
 getCoreState()->activeEditorView=NULL;completed_results_queue_shutdown();loop_events_shutdown();mainthread_context_clear_owner();SDL_Quit();puts("real event-loop edit/close/reopen/order/workspace acceptance: PASS");return 0;
}
