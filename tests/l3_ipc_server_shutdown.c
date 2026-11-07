#include <assert.h>
#include IPC_SERVER_SOURCE
static int connect_client(void) {
 int fd=socket(AF_UNIX,SOCK_STREAM,0);assert(fd>=0);struct sockaddr_un a={0};a.sun_family=AF_UNIX;strcpy(a.sun_path,g_server.socket_path);assert(!connect(fd,(struct sockaddr*)&a,sizeof(a)));return fd;
}
int main(void){
 assert(ide_ipc_start("/"));int clients[IPC_CLIENT_CAPACITY];
 for(int i=0;i<IPC_CLIENT_CAPACITY;i++){clients[i]=connect_client();assert(write(clients[i],"{",1)==1);}
 size_t count=0;
 for(int n=0;n<2000;n++){count=0;pthread_mutex_lock(&ipc_clients_lock);for(int i=0;i<IPC_CLIENT_CAPACITY;i++)count+=ipc_clients[i].used;pthread_mutex_unlock(&ipc_clients_lock);if(count==IPC_CLIENT_CAPACITY)break;usleep(1000);}
 assert(count==IPC_CLIENT_CAPACITY);ide_ipc_stop();for(int i=0;i<IPC_CLIENT_CAPACITY;i++){assert(!ipc_clients[i].used);close(clients[i]);}assert(!ide_ipc_is_running());ide_ipc_stop();
 assert(ide_ipc_start("/"));int fd=connect_client();const char*q="{\"proto\":1,\"id\":\"l3\",\"cmd\":\"ping\",\"args\":{}}\n";assert(write(fd,q,strlen(q))==(ssize_t)strlen(q));char reply[4096];ssize_t n=read(fd,reply,sizeof(reply)-1);assert(n>0);reply[n]=0;assert(strstr(reply,"true"));close(fd);ide_ipc_stop();puts("real IPC full-client shutdown and restart/ping: PASS");return 0;
}
