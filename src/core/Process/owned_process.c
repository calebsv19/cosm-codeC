#include "owned_process.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static bool collect_child(OwnedProcess* p, int flags) {
    if (p->reaped) return true;
    pid_t done;
    do { done=waitpid(p->pid,&p->status,flags); } while(done<0 && errno==EINTR);
    if (done==p->pid || (done<0 && errno==ECHILD)) {
        if (done<0) p->status=1<<8;
        p->reaped=true;
        kill(-p->pid,SIGKILL); /* descendants cannot extend this command lifetime */
        return true;
    }
    return false;
}

bool owned_process_start(const char* dir,const char* const argv[],int mode,OwnedProcess* out) {
    if (!out) return false;
    memset(out,0,sizeof(*out));out->pid=-1;
    if (!dir || !*dir || !argv || !argv[0] || !*argv[0]) return false;
    int fds[2];if(pipe(fds))return false;
    fcntl(fds[0],F_SETFD,FD_CLOEXEC);fcntl(fds[1],F_SETFD,FD_CLOEXEC);
    pid_t pid=fork();
    if(pid<0){close(fds[0]);close(fds[1]);return false;}
    if(!pid){
        setpgid(0,0);close(fds[0]);
        if(chdir(dir)||dup2(fds[1],STDOUT_FILENO)<0)_exit(127);
        if(mode==1 && dup2(fds[1],STDERR_FILENO)<0)_exit(127);
        if(mode==2){int n=open("/dev/null",O_WRONLY);if(n<0||dup2(n,2)<0)_exit(127);close(n);}
        close(fds[1]);execvp(argv[0],(char*const*)argv);_exit(127);
    }
    setpgid(pid,pid);close(fds[1]);out->pid=pid;
    int flags=fcntl(fds[0],F_GETFL,0);
    if(flags>=0 && !fcntl(fds[0],F_SETFL,flags|O_NONBLOCK)) out->stream=fdopen(fds[0],"r");
    if(!out->stream){close(fds[0]);kill(-pid,SIGKILL);kill(pid,SIGKILL);collect_child(out,0);out->pid=-1;return false;}
    return true;
}

bool owned_process_shell(const char* command,OwnedProcess* out) {
    const char* argv[]={"/bin/sh","-c",command,NULL};
    return command && owned_process_start(".",argv,1,out);
}

char* owned_process_read_line(OwnedProcess* p,char* line,size_t cap) {
    if(!p||!p->stream||!line||cap<2||p->eof)return NULL;
    collect_child(p,WNOHANG);
    size_t n=0;
    while(n+1<cap){
        errno=0;int c=fgetc(p->stream);
        if(c!=EOF){line[n++]=(char)c;if(c=='\n')break;continue;}
        int err=errno;
        if(feof(p->stream)){p->eof=true;break;}
        clearerr(p->stream);
        if(err==EINTR)continue;
        if(err!=EAGAIN && err!=EWOULDBLOCK){p->eof=true;break;}
        if(collect_child(p,WNOHANG)){
            /* Drain bytes already buffered/available once after exit, never
             * wait for a descendant (even one that escaped the process group). */
            c=fgetc(p->stream);
            if(c!=EOF){line[n++]=(char)c;if(c=='\n')break;continue;}
            p->eof=true;break;
        }
        struct pollfd fd={fileno(p->stream),POLLIN,0};poll(&fd,1,10);
    }
    line[n]=0;return n?line:NULL;
}

int owned_process_close(OwnedProcess* p) {
    if(!p||p->pid<=0)return 1;
    if(!p->eof && !p->reaped){kill(-p->pid,SIGKILL);kill(p->pid,SIGKILL);}
    if(p->stream){fclose(p->stream);p->stream=NULL;}
    bool ok=collect_child(p,0);p->pid=-1;p->eof=true;
    if(!ok)return 1;
    if(WIFEXITED(p->status))return WEXITSTATUS(p->status);
    return WIFSIGNALED(p->status)?128+WTERMSIG(p->status):1;
}
