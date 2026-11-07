/* Private bounded client ownership, included by the server and lifetime probe.
 * Only the accept owner reaps/reuses slots; stop runs after accept has joined.
 * Workers close their fd under the same lock used to interrupt blocked sockets. */
#define IPC_CLIENT_CAPACITY 16
static struct {
    pthread_t thread;
    int fd;
    bool used, done;
    void (*handle)(int);
} ipc_clients[IPC_CLIENT_CAPACITY];
static pthread_mutex_t ipc_clients_lock = PTHREAD_MUTEX_INITIALIZER;
static bool ipc_clients_accepting;
static void* ipc_client_run(void* arg) {
    size_t i = (size_t)arg;
    ipc_clients[i].handle(ipc_clients[i].fd);
    pthread_mutex_lock(&ipc_clients_lock);
    close(ipc_clients[i].fd);
    ipc_clients[i].fd = -1;
    ipc_clients[i].done = true;
    pthread_mutex_unlock(&ipc_clients_lock);
    return NULL;
}
static void ipc_clients_start(void) {
    pthread_mutex_lock(&ipc_clients_lock);
    ipc_clients_accepting = true;
    pthread_mutex_unlock(&ipc_clients_lock);
}
static bool ipc_clients_admit(int fd, void (*handle)(int)) {
    for (size_t i = 0; i < IPC_CLIENT_CAPACITY; ++i) {
        pthread_mutex_lock(&ipc_clients_lock);
        bool reap = ipc_clients[i].used && ipc_clients[i].done;
        pthread_mutex_unlock(&ipc_clients_lock);
        if (reap) {
            pthread_join(ipc_clients[i].thread, NULL);
            pthread_mutex_lock(&ipc_clients_lock);
            ipc_clients[i].used = false;
            pthread_mutex_unlock(&ipc_clients_lock);
        }
    }
    pthread_mutex_lock(&ipc_clients_lock);
    for (size_t i = 0; ipc_clients_accepting && i < IPC_CLIENT_CAPACITY; ++i) {
        if (ipc_clients[i].used) continue;
        ipc_clients[i].fd = fd; ipc_clients[i].handle = handle;
        ipc_clients[i].done = false; ipc_clients[i].used = true;
        if (pthread_create(&ipc_clients[i].thread, NULL, ipc_client_run, (void*)i) != 0) {
            ipc_clients[i].used = false;
            pthread_mutex_unlock(&ipc_clients_lock); return false;
        }
        pthread_mutex_unlock(&ipc_clients_lock); return true;
    }
    pthread_mutex_unlock(&ipc_clients_lock); return false;
}
static void ipc_clients_interrupt(void) {
    pthread_mutex_lock(&ipc_clients_lock);
    ipc_clients_accepting = false;
    for (size_t i = 0; i < IPC_CLIENT_CAPACITY; ++i)
        if (ipc_clients[i].used && !ipc_clients[i].done)
            shutdown(ipc_clients[i].fd, SHUT_RDWR);
    pthread_mutex_unlock(&ipc_clients_lock);
}
static void ipc_clients_join(void) {
    for (size_t i = 0; i < IPC_CLIENT_CAPACITY; ++i) {
        if (!ipc_clients[i].used) continue;
        pthread_join(ipc_clients[i].thread, NULL);
        ipc_clients[i].used = false;
    }
}
