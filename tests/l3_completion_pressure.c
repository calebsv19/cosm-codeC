#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
static int fail_alloc,freed,bad,fail_mutex;
static SDL_mutex *create_mutex(void){return fail_mutex?NULL:SDL_CreateMutex();}
static void *allocate(size_t n,size_t z){return fail_alloc?NULL:calloc(n,z);}
#define SDL_CreateMutex create_mutex
#define calloc allocate
#ifndef QUEUE_SOURCE
#define QUEUE_SOURCE "../src/core/LoopResults/completed_results_queue.c"
#endif
#include QUEUE_SOURCE
#undef calloc
#undef SDL_CreateMutex
static void release_payload(void *p){freed++;free(p);}
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);bad++;}}while(0)
int main(void){
 fail_mutex=1;completed_results_queue_init();CHECK(!g_initialized);fail_mutex=0;
 completed_results_queue_init();
 for(int sub=1;sub<=3;sub++)for(int i=0;i<512;i++){CompletedResult r={0};r.subsystem=sub;r.kind=COMPLETED_RESULT_ANALYSIS_PROGRESS;CHECK(completed_results_queue_push(&r));}
 CompletedResult terminal={0};terminal.subsystem=COMPLETED_SUBSYSTEM_ANALYSIS;terminal.kind=COMPLETED_RESULT_ANALYSIS_FINISHED;terminal.payload.analysis_finished.analysis_run_id=42;terminal.owned_ptr=malloc(16);terminal.free_owned_ptr=release_payload;memset(terminal.owned_ptr,0x5a,16);
 fail_alloc=1;int accepted=completed_results_queue_push(&terminal);CHECK(accepted);fail_alloc=0;
 if(accepted){CompletedResult duplicate=terminal;duplicate.owned_ptr=malloc(16);int prior=freed;CHECK(!completed_results_queue_push(&duplicate));CHECK(freed==prior+1);}
 /* Refused allocation must consume payload just like queue-full refusal. */
 CompletedResult refused={0};refused.subsystem=COMPLETED_SUBSYSTEM_SYMBOLS;refused.kind=COMPLETED_RESULT_SYMBOLS_UPDATED;refused.owned_ptr=malloc(16);refused.free_owned_ptr=release_payload;int before=freed;fail_alloc=1;CHECK(!completed_results_queue_push(&refused));fail_alloc=0;CHECK(freed==before+1);if(freed==before)free(refused.owned_ptr);
 if(!accepted)free(terminal.owned_ptr);
 CompletedResult out,held={0};uint64_t seq=0;int count=0,terminals=0;
 while(completed_results_queue_pop_any(&out)){CHECK(out.seq>seq);seq=out.seq;count++;if(out.kind==COMPLETED_RESULT_ANALYSIS_FINISHED){held=out;terminals++;}else completed_results_queue_release(&out);}
 CHECK(count==1537&&terminals==1);completed_results_queue_shutdown();CHECK(!held.owned_ptr||(held.payload.analysis_finished.analysis_run_id==42&&((unsigned char*)held.owned_ptr)[0]==0x5a));completed_results_queue_release(&held);
 completed_results_queue_init();terminal.owned_ptr=NULL;CHECK(completed_results_queue_push(&terminal));completed_results_queue_reset();CHECK(!completed_results_queue_pop_any(&out));completed_results_queue_shutdown();
 printf("completion pressure: %s\n",bad?"FAIL":"PASS");return bad?1:0;
}
