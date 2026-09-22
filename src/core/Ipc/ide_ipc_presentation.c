#include "core/Ipc/ide_ipc_presentation.h"

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t cond;
    bool inflight;
    bool pending;
    bool completed;
    bool success;
    uint64_t generation;
    char text[IDE_IPC_PRESENTATION_MAX_BYTES + 1];
    char error[160];
    IdeIpcPresentationHandler handler;
    void* userdata;
} PresentationState;

static PresentationState g_presentation = {
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .cond = PTHREAD_COND_INITIALIZER,
};

static void set_error(char* out, size_t cap, const char* message) {
    if (out && cap) snprintf(out, cap, "%s", message);
}

static bool valid_text(const char* text, size_t* length_out) {
    if (!text) return false;
    size_t length = strnlen(text, IDE_IPC_PRESENTATION_MAX_BYTES + 1);
    if (length == 0 || length > IDE_IPC_PRESENTATION_MAX_BYTES) return false;
    for (size_t i = 0; i < length; ++i) {
        unsigned char c = (unsigned char)text[i];
        if (c < 32 && c != '\n' && c != '\t') return false;
        if (c == 127) return false;
    }
    if (length_out) *length_out = length;
    return true;
}

void ide_ipc_presentation_set_handler(IdeIpcPresentationHandler handler, void* userdata) {
    pthread_mutex_lock(&g_presentation.lock);
    g_presentation.handler = handler;
    g_presentation.userdata = userdata;
    pthread_mutex_unlock(&g_presentation.lock);
}

bool ide_ipc_presentation_request(const char* text, char* error_out, size_t error_out_cap) {
    size_t length = 0;
    if (!valid_text(text, &length)) {
        set_error(error_out, error_out_cap, "Presentation text must be 1-4096 printable bytes");
        return false;
    }

    pthread_mutex_lock(&g_presentation.lock);
    if (!g_presentation.handler || g_presentation.inflight) {
        set_error(error_out, error_out_cap,
                  g_presentation.handler ? "Presentation is busy" : "Presentation handler unavailable");
        pthread_mutex_unlock(&g_presentation.lock);
        return false;
    }
    memcpy(g_presentation.text, text, length + 1);
    g_presentation.inflight = true;
    g_presentation.generation++;
    g_presentation.pending = true;
    g_presentation.completed = false;
    g_presentation.success = false;
    g_presentation.error[0] = '\0';

    struct timespec deadline;
    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec += 5;
    while (!g_presentation.completed) {
        int rc = pthread_cond_timedwait(&g_presentation.cond, &g_presentation.lock, &deadline);
        if (rc == ETIMEDOUT) {
            g_presentation.pending = false;
            g_presentation.inflight = false;
            set_error(error_out, error_out_cap, "Presentation timed out; inspect the IDE before retrying");
            pthread_mutex_unlock(&g_presentation.lock);
            return false;
        }
    }
    bool success = g_presentation.success;
    if (!success) set_error(error_out, error_out_cap, g_presentation.error);
    g_presentation.inflight = false;
    pthread_mutex_unlock(&g_presentation.lock);
    return success;
}

void ide_ipc_presentation_pump(void) {
    char text[IDE_IPC_PRESENTATION_MAX_BYTES + 1];
    IdeIpcPresentationHandler handler = NULL;
    void* userdata = NULL;
    uint64_t generation = 0;
    pthread_mutex_lock(&g_presentation.lock);
    if (g_presentation.pending && g_presentation.inflight) {
        memcpy(text, g_presentation.text, sizeof(text));
        handler = g_presentation.handler;
        userdata = g_presentation.userdata;
        generation = g_presentation.generation;
        g_presentation.pending = false;
    }
    pthread_mutex_unlock(&g_presentation.lock);
    if (!handler) return;

    char error[160] = {0};
    bool success = handler(text, error, sizeof(error), userdata);
    pthread_mutex_lock(&g_presentation.lock);
    if (g_presentation.inflight && g_presentation.generation == generation) {
        g_presentation.success = success;
        g_presentation.completed = true;
        set_error(g_presentation.error, sizeof(g_presentation.error), error);
        pthread_cond_broadcast(&g_presentation.cond);
    }
    pthread_mutex_unlock(&g_presentation.lock);
}

void ide_ipc_presentation_cancel(void) {
    pthread_mutex_lock(&g_presentation.lock);
    g_presentation.pending = false;
    g_presentation.completed = true;
    g_presentation.success = false;
    set_error(g_presentation.error, sizeof(g_presentation.error), "IDE session stopped");
    pthread_cond_broadcast(&g_presentation.cond);
    pthread_mutex_unlock(&g_presentation.lock);
}
