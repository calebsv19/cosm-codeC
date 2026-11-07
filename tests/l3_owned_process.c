#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
static int fail_pipe,fail_fork,fail_stream,interrupt_wait;
static int probe_pipe(int p[2]){if(fail_pipe){errno=EMFILE;return -1;}return pipe(p);}
static pid_t probe_fork(void){if(fail_fork){errno=EAGAIN;return -1;}return fork();}
static FILE*probe_fdopen(int fd,const char*m){return fail_stream?NULL:fdopen(fd,m);}
static pid_t probe_wait(pid_t p,int*s,int f){if(interrupt_wait){interrupt_wait=0;errno=EINTR;return -1;}return waitpid(p,s,f);}
#define pipe probe_pipe
#define fork probe_fork
#define fdopen probe_fdopen
#define waitpid probe_wait
#include "../src/core/Process/owned_process.c"
#undef pipe
#undef fork
#undef fdopen
#undef waitpid
static void run(const char*cmd,int expected,const char*prefix){OwnedProcess p;assert(owned_process_shell(cmd,&p));pid_t pid=p.pid;char line[32],all[128]={0};while(owned_process_read_line(&p,line,sizeof(line)))strncat(all,line,sizeof(all)-strlen(all)-1);assert(!strncmp(all,prefix,strlen(prefix)));interrupt_wait=1;int rc=owned_process_close(&p);fprintf(stderr,"case %s: status %d expected %d\n",cmd,rc,expected);assert(rc==expected);errno=0;assert(waitpid(pid,NULL,WNOHANG)==-1&&errno==ECHILD);assert(owned_process_close(&p)==1);}
int main(void){
 OwnedProcess p;fail_pipe=1;assert(!owned_process_shell("exit 0",&p));fail_pipe=0;fail_fork=1;assert(!owned_process_shell("exit 0",&p));fail_fork=0;fail_stream=1;assert(!owned_process_shell("sleep 30",&p));fail_stream=0;
 run("printf 'ordinary\\nsecond\\n'; exit 23",23,"ordinary\nsecond\n");
 run("sleep 30 & printf prefix; exit 0",0,"prefix");
 run("trap '' TERM; sleep 30 & printf tail; exit 7",7,"tail");
 run("exec 1>&-; sleep 0.05; exit 19",19,"");
 const char*missing[]={"/definitely-missing-l3",NULL};assert(owned_process_start("/",missing,1,&p));char line[32];while(owned_process_read_line(&p,line,sizeof(line))){}assert(owned_process_close(&p)==127);
 assert(owned_process_shell("trap '' TERM; sleep 30",&p));pid_t cancelled=p.pid;assert(owned_process_close(&p)!=0);errno=0;assert(waitpid(cancelled,NULL,WNOHANG)==-1&&errno==ECHILD);
 assert(owned_process_shell("sleep 30 & echo $!; exit 0",&p));assert(owned_process_read_line(&p,line,sizeof(line)));pid_t descendant=(pid_t)strtol(line,NULL,10);assert(descendant>1);while(owned_process_read_line(&p,line,sizeof(line))){}assert(owned_process_close(&p)==0);for(int i=0;i<2000&&kill(descendant,0)==0;i++)usleep(1000);errno=0;assert(kill(descendant,0)<0&&errno==ESRCH);
 puts("owned process admission, EOF/exit, descendants, status, EINTR and reuse: PASS");return 0;
}
