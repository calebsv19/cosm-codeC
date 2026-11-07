#include <SDL2/SDL.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
static SDL_sem *entered,*release_io,*joining;
static int creates,joins,reject_create,hold=1,bad,fail_alloc;
static void *test_malloc(size_t n){return fail_alloc?NULL:malloc(n);}
static int blocked_open(const char *path,int flags,...){if(hold){SDL_SemPost(entered);SDL_SemWait(release_io);}return open(path,flags);}
static int counted_create(pthread_t *t,const pthread_attr_t *a,void *(*fn)(void*),void *u){creates++;return reject_create?EAGAIN:pthread_create(t,a,fn,u);}
static int counted_join(pthread_t t,void **result){joins++;SDL_SemPost(joining);return pthread_join(t,result);}
#define malloc test_malloc
#define open blocked_open
#define pthread_create counted_create
#define pthread_join counted_join
#include CACHE_SOURCE
#undef malloc
#undef open
#undef pthread_create
#undef pthread_join
#ifdef BUILD_CACHE
#define LOAD build_diagnostics_load
#define STOP build_diagnostics_cache_shutdown
#define INIT build_diagnostics_cache_init
#define POLL build_diagnostics_poll
#else
#define LOAD diagnostics_load
#define STOP diagnostics_cache_shutdown
#define INIT initDiagnosticsEngine
#define POLL diagnostics_poll
#endif
#ifdef BASELINE
static void missing_shutdown(void){}
#undef STOP
#define STOP missing_shutdown
#ifdef BUILD_CACHE
static void missing_init(void){}
#undef INIT
#define INIT missing_init
#endif
#endif
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);bad++;}}while(0)
static int unblock(void *u){(void)u;SDL_SemWaitTimeout(joining,500);SDL_SemPost(release_io);SDL_SemPost(release_io);return 0;}
int main(int argc,char **argv){if(argc!=2)return 2;SDL_Init(SDL_INIT_TIMER);entered=SDL_CreateSemaphore(0);release_io=SDL_CreateSemaphore(0);joining=SDL_CreateSemaphore(0);INIT();
 LOAD(argv[1]);LOAD(argv[1]);CHECK(SDL_SemWaitTimeout(entered,1000)==0);CHECK(SDL_SemWaitTimeout(entered,1000)==0);LOAD(argv[1]);CHECK(creates==2);
 SDL_Thread *helper=SDL_CreateThread(unblock,"unblock",NULL);STOP();CHECK(joins==2);SDL_WaitThread(helper,NULL);
#ifdef BASELINE
 /* Retire detached baseline readers before fixture destruction. */
 for(int n=0;n<1000;n++){POLL();if(!cache_reads[0].busy&&!cache_reads[1].busy)break;SDL_Delay(1);}
#else
 CHECK(!cache_reads[0].busy&&!cache_reads[1].busy);int prior=creates;LOAD(argv[1]);CHECK(creates==prior);STOP();CHECK(joins==2);
 hold=0;INIT();reject_create=1;LOAD(argv[1]);CHECK(!cache_reads[0].busy&&!cache_reads[1].busy);reject_create=0;
 LOAD(argv[1]);for(int n=0;n<1000&&cache_reads[0].busy;n++){POLL();SDL_Delay(1);}CHECK(!cache_reads[0].busy);CHECK(joins==3);STOP();
 INIT();fail_alloc=1;LOAD(argv[1]);STOP();fail_alloc=0;CHECK(joins==4&&!cache_reads[0].busy&&!cache_reads[1].busy);
#endif
 SDL_DestroySemaphore(entered);SDL_DestroySemaphore(release_io);SDL_DestroySemaphore(joining);SDL_Quit();printf("cache shutdown: %s\n",bad?"FAIL":"PASS");return bad?1:0;
}
