#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
static int interrupt_wait;
static pid_t test_waitpid(pid_t p,int*s,int flags) {
 if(interrupt_wait) {interrupt_wait=0;errno=EINTR;return -1;}
 return waitpid(p,s,flags);
}
#define waitpid test_waitpid
#include "../src/core/Process/owned_process.c"
#include GIT_SOURCE
#include TERMINAL_SOURCE
#undef waitpid
static int failed;
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);failed++;}}while(0)
int main(void) {
 const char* argv[]={"/bin/sh","-c","exit 23",NULL};GitCommandProcess p;
 assert(git_command_process_start("/",argv,GIT_COMMAND_STDERR_TO_STDOUT,&p));
 pid_t child=p.pid;char drained[32];while(owned_process_read_line(&p,drained,sizeof(drained))){}interrupt_wait=1;CHECK(git_command_process_close(&p)==23);
 errno=0;CHECK(waitpid(child,NULL,WNOHANG)==-1&&errno==ECHILD);
 if(waitpid(child,NULL,WNOHANG)==0){kill(child,SIGKILL);waitpid(child,NULL,0);}
 int ready[2];assert(pipe(ready)==0);child=fork();assert(child>=0);
 if(!child){close(ready[0]);setsid();signal(SIGHUP,SIG_IGN);signal(SIGTERM,SIG_IGN);write(ready[1],"x",1);for(;;)pause();}
 close(ready[1]);char b;assert(read(ready[0],&b,1)==1);close(ready[0]);
 TerminalBackend*t=calloc(1,sizeof(*t));t->master_fd=-1;t->child_pid=child;t->dead=true;
 terminal_backend_destroy(t);errno=0;CHECK(waitpid(child,NULL,WNOHANG)==-1&&errno==ECHILD);
 if(kill(child,0)==0){kill(-child,SIGKILL);kill(child,SIGKILL);waitpid(child,NULL,0);}
 assert(pipe(ready)==0);child=fork();assert(child>=0);
 if(!child){close(ready[0]);setsid();pid_t descendant=fork();if(!descendant){for(;;)pause();}write(ready[1],&descendant,sizeof(descendant));_exit(0);}
 close(ready[1]);pid_t descendant;assert(read(ready[0],&descendant,sizeof(descendant))==sizeof(descendant));close(ready[0]);
 t=calloc(1,sizeof(*t));t->master_fd=-1;t->child_pid=child;
 for(int i=0;i<2000&&terminal_backend_poll_child(t)==0;i++)usleep(1000);
 for(int i=0;i<2000&&kill(descendant,0)==0;i++)usleep(1000);
 errno=0;CHECK(kill(descendant,0)<0&&errno==ESRCH);terminal_backend_destroy(t);
 puts(failed?"process ownership: FAIL":"process ownership: PASS");return failed?1:0;
}
