/* Actual production worker/start/stop with a deterministic scan-entry barrier. */
#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>
#include "core/Analysis/include_path_resolver.h"
static SDL_sem *entered,*release_worker,*join_entered;

static int fail_create,create_calls;
static size_t blocked_flags(const char *root,const char *args,BuildFlagSet *flags){
 (void)root;(void)args;memset(flags,0,sizeof(*flags));SDL_SemPost(entered);SDL_SemWait(release_worker);return 0;
}
static void joined(SDL_Thread *t,int *status){SDL_SemPost(join_entered);SDL_WaitThread(t,status);}
static SDL_Thread *create_worker(SDL_ThreadFunction fn,const char *name,size_t stack,void *arg){create_calls++;return fail_create?NULL:SDL_CreateThreadWithStackSize(fn,name,stack,arg);}
#define gather_build_flags blocked_flags
#define SDL_WaitThread joined
#define SDL_CreateThreadWithStackSize create_worker
#ifndef ANALYSIS_JOB_SOURCE
#define ANALYSIS_JOB_SOURCE "../src/core/Analysis/analysis_job.c"
#endif
#include ANALYSIS_JOB_SOURCE
#undef SDL_CreateThreadWithStackSize
#undef SDL_WaitThread
#undef gather_build_flags
static int unlock_worker(void *unused){
 (void)unused;
 int saw_join=SDL_SemWaitTimeout(join_entered,1000)==0;
 if(!saw_join)fprintf(stderr,"FAIL: shutdown never joined worker\n");
 int cancelled=analysis_job_cancel_requested();
 analysis_job_request_cancel(); /* Also safely retire the deliberately broken baseline. */
 SDL_SemPost(release_worker);return saw_join&&cancelled?0:1;
}
int main(void){
 int bad=0;SDL_Init(SDL_INIT_TIMER);entered=SDL_CreateSemaphore(0);release_worker=SDL_CreateSemaphore(0);join_entered=SDL_CreateSemaphore(0);
 analysis_job_system_init();analysis_status_init();completed_results_queue_init();
 for(int live=0;live<2;live++){
  completed_results_queue_reset();
  analysis_job_system_init();
  if(live)start_async_live_buffer_analysis("/tmp/owned-test","",2,"/tmp/owned-test/a.c","int x;",6,1);
  else start_async_workspace_analysis("/tmp/owned-test","",1);
  if(SDL_SemWaitTimeout(entered,2000)!=0)return 2;
  SDL_Thread *releaser=SDL_CreateThread(unlock_worker,"release",NULL);
  analysis_job_system_shutdown();
  if(analysis_job_is_running()){fprintf(stderr,"FAIL: worker still running after shutdown\n");bad++;}
  int result;SDL_WaitThread(releaser,&result);bad+=result;
  /* Baseline detached worker must finish before fixture storage is freed. */
  for(int n=0;n<2000&&analysis_job_is_running();n++)SDL_Delay(1);
  analysis_job_system_shutdown();
 }

 /* Closed admission cannot restart either API, including a failed launch. */
 fail_create=1;int before=create_calls;
 start_async_workspace_analysis("/tmp/owned-test","",3);
 start_async_live_buffer_analysis("/tmp/owned-test","",4,"/tmp/owned-test/a.c","x",1,2);
 if(create_calls!=before){fprintf(stderr,"FAIL: shutdown admitted new work\n");bad++;}
 completed_results_queue_reset();analysis_job_system_init();start_async_live_buffer_analysis("/tmp/owned-test","",5,"/tmp/owned-test/a.c","x",1,2);
 if(analysis_job_is_running()||g_live_contents||g_thread){fprintf(stderr,"FAIL: failed launch retained ownership\n");bad++;}
 completed_results_queue_reset();fail_create=0;
 start_async_workspace_analysis("/tmp/owned-test","",6);
 if(SDL_SemWaitTimeout(entered,2000)!=0)return 2;
 analysis_job_request_cancel();SDL_SemPost(release_worker);
 for(int n=0;n<2000&&analysis_job_is_running();n++){analysis_job_poll();SDL_Delay(1);}
 if(analysis_job_is_running()||g_thread){fprintf(stderr,"FAIL: poll did not join completion\n");bad++;}
 analysis_job_system_shutdown();analysis_job_system_shutdown();
 completed_results_queue_shutdown();
 SDL_DestroySemaphore(entered);SDL_DestroySemaphore(release_worker);SDL_DestroySemaphore(join_entered);SDL_Quit();
 printf("analysis shutdown workspace/live: %s\n",bad?"FAIL":"PASS");return bad?1:0;
}
