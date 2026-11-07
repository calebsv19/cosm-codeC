#include <assert.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
static void build_diagnostics_feed_chunk(const char*p,size_t n){(void)p;(void)n;}
static int refuse_thread;
static int create_client(pthread_t*t,const pthread_attr_t*a,void*(*fn)(void*),void*arg){return refuse_thread?EAGAIN:pthread_create(t,a,fn,arg);}
#define pthread_create create_client
#include "core/Ipc/ide_ipc_clients.inc"
#undef pthread_create
#include "core/Ipc/ide_ipc_capture.inc"
static atomic_int entries, exits;
static void blocked_client(int fd){atomic_fetch_add(&entries,1);char b;read(fd,&b,1);atomic_fetch_add(&exits,1);}
static void blocked_reply(int fd){char buf[8192];memset(buf,'x',sizeof(buf));atomic_fetch_add(&entries,1);while(write(fd,buf,sizeof(buf))>0){}atomic_fetch_add(&exits,1);}
static void* stop_build(void*x){(void)x;usleep(100000);ide_ipc_build_set_stopping(true);return NULL;}
int main(void){
 int peers[IPC_CLIENT_CAPACITY];ipc_clients_start();
 int refused[2];assert(!socketpair(AF_UNIX,SOCK_STREAM,0,refused));refuse_thread=1;assert(!ipc_clients_admit(refused[0],blocked_client));refuse_thread=0;close(refused[0]);close(refused[1]);
 for(int i=0;i<IPC_CLIENT_CAPACITY;i++){int p[2];assert(!socketpair(AF_UNIX,SOCK_STREAM,0,p));assert(ipc_clients_admit(p[0],blocked_client));peers[i]=p[1];}
 int extra[2];assert(!socketpair(AF_UNIX,SOCK_STREAM,0,extra));assert(!ipc_clients_admit(extra[0],blocked_client));close(extra[0]);close(extra[1]);
 ipc_clients_interrupt();ipc_clients_join();assert(atomic_load(&entries)==IPC_CLIENT_CAPACITY&&atomic_load(&exits)==IPC_CLIENT_CAPACITY);
 for(int i=0;i<IPC_CLIENT_CAPACITY;i++)close(peers[i]);
 ipc_clients_join();ipc_clients_start();int p[2];assert(!socketpair(AF_UNIX,SOCK_STREAM,0,p));assert(ipc_clients_admit(p[0],blocked_client));ipc_clients_interrupt();ipc_clients_join();close(p[1]);
 ipc_clients_start();assert(!socketpair(AF_UNIX,SOCK_STREAM,0,p));int one=1;setsockopt(p[0],SOL_SOCKET,SO_NOSIGPIPE,&one,sizeof(one));assert(ipc_clients_admit(p[0],blocked_reply));usleep(20000);ipc_clients_interrupt();ipc_clients_join();close(p[1]);
 char out[64];size_t len;char*ordinary[]={"/bin/sh","-c","printf done; exit 23",NULL};assert(run_exec_capture("/",ordinary,NULL,out,sizeof(out),&len)==23&&!strcmp(out,"done"));
 char*descendant[]={"/bin/sh","-c","sleep 20 & printf prefix; exit 0",NULL};assert(run_exec_capture("/",descendant,NULL,out,sizeof(out),&len)==0&&!strcmp(out,"prefix"));
 pthread_t stop;assert(!pthread_create(&stop,NULL,stop_build,NULL));char*hang[]={"/bin/sh","-c","trap '' TERM; sleep 20",NULL};assert(run_exec_capture("/",hang,NULL,out,sizeof(out),&len)==-1);pthread_join(stop,NULL);
 assert(run_exec_capture("/",ordinary,NULL,out,sizeof(out),&len)==-1);ide_ipc_build_set_stopping(false);assert(run_exec_capture("/",ordinary,NULL,out,sizeof(out),&len)==23);
 puts("IPC capacity, socket shutdown, child exit/EOF, cancellation and reuse: PASS");return 0;
}
