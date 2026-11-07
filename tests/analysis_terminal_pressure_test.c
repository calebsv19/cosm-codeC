/* Reuse the deterministic entry barrier around the actual worker. */
#define main shutdown_test_main
#include "analysis_job_shutdown_test.c"
#undef main
int main(void){
 int bad=0;SDL_Init(SDL_INIT_TIMER);entered=SDL_CreateSemaphore(0);release_worker=SDL_CreateSemaphore(0);join_entered=SDL_CreateSemaphore(0);
 analysis_status_init();analysis_job_system_init();
 int initial=create_calls;start_async_workspace_analysis("/tmp/owned-test","",90);if(create_calls!=initial)bad++;
 completed_results_queue_init();
 for(int i=0;i<512;i++){CompletedResult r={0};r.subsystem=COMPLETED_SUBSYSTEM_ANALYSIS;r.kind=COMPLETED_RESULT_ANALYSIS_PROGRESS;if(!completed_results_queue_push(&r))bad++;}
 start_async_workspace_analysis("/tmp/owned-test","",91);if(SDL_SemWaitTimeout(entered,2000)!=0)return 2;
 analysis_job_request_cancel();SDL_SemPost(release_worker);
 for(int n=0;n<2000&&analysis_job_is_running();n++){analysis_job_poll();SDL_Delay(1);}
 if(analysis_job_is_running()||!analysis_refresh_running()||!completed_results_queue_analysis_terminal_pending())bad++;
 int before=create_calls;start_async_workspace_analysis("/tmp/owned-test","",92);if(create_calls!=before)bad++;
 CompletedResult r;int terminals=0;
 while(completed_results_queue_pop_any(&r)){if(r.kind==COMPLETED_RESULT_ANALYSIS_FINISHED){terminals++;if(r.payload.analysis_finished.analysis_run_id!=91||!r.payload.analysis_finished.cancelled)bad++;}completed_results_queue_release(&r);}
 analysis_job_poll();if(terminals!=1||analysis_refresh_running())bad++;
 fail_create=1;start_async_live_buffer_analysis("/tmp/owned-test","",93,"/tmp/owned-test/a.c","x",1,1);
 terminals=0;while(completed_results_queue_pop_any(&r)){if(r.kind==COMPLETED_RESULT_ANALYSIS_FINISHED){terminals++;if(!r.payload.analysis_finished.had_error||r.payload.analysis_finished.analysis_run_id!=93)bad++;}completed_results_queue_release(&r);}
 if(terminals!=1)bad++;analysis_job_poll();analysis_job_system_shutdown();completed_results_queue_shutdown();SDL_DestroySemaphore(entered);SDL_DestroySemaphore(release_worker);SDL_DestroySemaphore(join_entered);SDL_Quit();printf("real analysis terminal pressure: %s\n",bad?"FAIL":"PASS");return bad?1:0;
}
