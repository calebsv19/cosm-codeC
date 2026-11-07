#include <assert.h>
#include "core/Analysis/analysis_job.h"
#include "ide/Panes/Editor/editor_view_file_helpers.h"
#include EVENT_LOOP_SOURCE
static void frame(FrameContext* ctx){invalidateAll(ctx->panes,*ctx->paneCount,RENDER_INVALIDATION_CONTENT);SDL_Delay(20);runFrameLoop(ctx,SDL_GetPerformanceCounter(),0.02f);}
static void capture(FrameContext* ctx,const char* path){
 RenderContext*r=getRenderContext();assert(vk_renderer_request_capture(r->renderer,path)==VK_SUCCESS);
 for(int i=0;i<4;i++)frame(ctx);
 struct stat st;assert(!stat(path,&st)&&st.st_size>0);
}
int main(int argc,char**argv){
 assert(argc==1);STARTING_WIDTH=1280;STARTING_HEIGHT=800;assert(initializeSystem(argv[0]));setTimerHudEnabled(false);
 analysis_job_system_shutdown();analysis_scheduler_init();completed_results_queue_reset();analysis_job_system_init();
 IDECoreState* core=getCoreState();if(!core->persistentEditorView)core->persistentEditorView=createEditorView();EditorView*view=core->persistentEditorView;setActiveEditorView(view);
 char path[1024];snprintf(path,sizeof(path),"%s/main.c",projectPath);OpenFile*file=openFileInView(view,path);assert(file);
 UIPane*panes[MAX_PANES];int count=0;layout_static_panes(panes,&count);ResizeZone zones[5];int zcount=0,w=0,h=0;bool running=true;SDL_Event event;Uint64 last=SDL_GetPerformanceCounter();FrameContext ctx={panes,&count,zones,&zcount,&running,&event,&w,&h,0.016f,&last};
 CompletedResult hint={0};hint.kind=COMPLETED_RESULT_ANALYSIS_PROGRESS;
 for(int subsystem=1;subsystem<=3;subsystem++){hint.subsystem=(CompletedResultSubsystem)subsystem;for(int i=0;i<512;i++)assert(completed_results_queue_push(&hint));}
 const char*text="int main(void) { return unknown_l3_name; }\n";
 start_async_live_buffer_analysis(projectPath,"",1000,path,text,strlen(text),file->documentRevision);
 for(int i=0;i<5000&&analysis_job_is_running();i++){analysis_job_poll();SDL_Delay(1);}assert(!analysis_job_is_running());assert(completed_results_queue_analysis_terminal_pending());
 for(int i=0;i<80&&completed_results_queue_analysis_terminal_pending();i++)frame(&ctx);
 assert(analysis_status_get()==ANALYSIS_STATUS_FRESH);assert(getDiagnosticCount()>0);printf("native pressure accepted: diagnostics=%d\n",getDiagnosticCount());
 capture(&ctx,getenv("L3_CAPTURE_ACCEPTED"));
 /* Reuse the actual tab/path, hold an old terminal, then replace its owner. */
 CompletedResult old={0};old.kind=COMPLETED_RESULT_ANALYSIS_FINISHED;old.subsystem=COMPLETED_SUBSYSTEM_ANALYSIS;old.workspace_generation=ide_ipc_workspace_generation();old.has_document_revision=true;strcpy(old.document_path,path);old.document_revision=file->documentRevision;old.payload.analysis_finished.analysis_run_id=1001;old.payload.analysis_finished.library_index_stamp=library_index_combined_stamp();strcpy(old.payload.analysis_finished.project_root,projectPath);
 closeTab(view,view->activeTab);file=openFileInView(view,path);assert(file&&file->documentRevision!=old.document_revision);assert(completed_results_queue_push(&old));
 CompletedResultsQueueStats before,after;completed_results_queue_snapshot(&before);frame(&ctx);completed_results_queue_snapshot(&after);assert(after.results_stale_dropped>before.results_stale_dropped);
 old.has_document_revision=false;old.payload.analysis_finished.analysis_run_id=1002;ide_ipc_workspace_changed();assert(completed_results_queue_push(&old));completed_results_queue_snapshot(&before);frame(&ctx);completed_results_queue_snapshot(&after);assert(after.results_stale_dropped>before.results_stale_dropped);
 capture(&ctx,getenv("L3_CAPTURE_REPLACED"));
 shutdownSystem(panes,count);puts("native pressure, same-path replacement, workspace lifetime and shutdown: PASS");return 0;
}
