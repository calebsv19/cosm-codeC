#include "core/LoopMessages/mainthread_message_queue.h"

#include <SDL2/SDL.h>
#include <stdlib.h>
#include <string.h>

#include "core_queue.h"

typedef struct MainThreadMessageNode {
    MainThreadMessage msg;
} MainThreadMessageNode;

enum { MAINTHREAD_MESSAGE_QUEUE_CAPACITY = 1024 };

static CoreQueueMutex g_queue;
static void* g_queue_backing[MAINTHREAD_MESSAGE_QUEUE_CAPACITY];
static bool g_queue_initialized = false;
static SDL_mutex* g_stats_mutex = NULL;

static uint64_t g_next_seq = 1;
static uint32_t g_high_watermark = 0;
static uint64_t g_pushed = 0;
static uint64_t g_popped = 0;
static uint64_t g_dropped_progress = 0;
static uint64_t g_replaced_git_snapshots = 0;
static Uint32 g_last_progress_push_ms = 0;

static void stats_lock(void) {
    if (g_stats_mutex) SDL_LockMutex(g_stats_mutex);
}

static void stats_unlock(void) {
    if (g_stats_mutex) SDL_UnlockMutex(g_stats_mutex);
}

static void release_owned(MainThreadMessage* msg) {
    if (!msg) return;
    if (msg->free_owned_ptr && msg->owned_ptr) {
        msg->free_owned_ptr(msg->owned_ptr);
    }
    msg->owned_ptr = NULL;
    msg->free_owned_ptr = NULL;
}

void mainthread_message_queue_init(void) {
    if (!g_stats_mutex) g_stats_mutex = SDL_CreateMutex();
    if (!g_queue_initialized) {
        g_queue_initialized = core_queue_mutex_init_ex(&g_queue,
                                                       g_queue_backing,
                                                       MAINTHREAD_MESSAGE_QUEUE_CAPACITY,
                                                       CORE_QUEUE_OVERFLOW_REJECT);
    }
    mainthread_message_queue_reset();
}

void mainthread_message_queue_shutdown(void) {
    mainthread_message_queue_reset();
    if (g_queue_initialized) {
        core_queue_mutex_destroy(&g_queue);
        g_queue_initialized = false;
    }
    if (g_stats_mutex) {
        SDL_DestroyMutex(g_stats_mutex);
        g_stats_mutex = NULL;
    }
}

void mainthread_message_queue_reset(void) {
    if (g_queue_initialized) {
        void* item = NULL;
        while (core_queue_mutex_pop(&g_queue, &item)) {
            MainThreadMessageNode* node = (MainThreadMessageNode*)item;
            if (node) {
                release_owned(&node->msg);
                free(node);
            }
        }
    }

    stats_lock();
    g_next_seq = 1;
    g_high_watermark = 0;
    g_pushed = 0;
    g_popped = 0;
    g_dropped_progress = 0;
    g_replaced_git_snapshots = 0;
    g_last_progress_push_ms = 0;
    stats_unlock();
}

static bool push_internal(const MainThreadMessage* msg) {
    if (!msg || msg->type == MAINTHREAD_MSG_NONE || !g_queue_initialized) return false;

    MainThreadMessageNode* node = (MainThreadMessageNode*)calloc(1, sizeof(*node));
    if (!node) return false;
    node->msg = *msg;

    stats_lock();
    node->msg.seq = g_next_seq++;
    stats_unlock();

    if (!core_queue_mutex_push(&g_queue, node)) {
        release_owned(&node->msg);
        free(node);
        return false;
    }

    stats_lock();
    g_pushed++;
    uint32_t depth = (uint32_t)core_queue_mutex_size(&g_queue);
    if (depth > g_high_watermark) g_high_watermark = depth;
    stats_unlock();
    return true;
}

bool mainthread_message_queue_push(const MainThreadMessage* msg) {
    return push_internal(msg);
}

bool mainthread_message_queue_push_progress_throttled(const MainThreadMessage* msg,
                                                      uint32_t min_interval_ms) {
    if (!msg || msg->type != MAINTHREAD_MSG_PROGRESS) return false;
    Uint32 now = SDL_GetTicks();
    stats_lock();
    if (min_interval_ms > 0 && (now - g_last_progress_push_ms) < min_interval_ms) {
        g_dropped_progress++;
        stats_unlock();
        return false;
    }
    g_last_progress_push_ms = now;
    stats_unlock();
    return push_internal(msg);
}

bool mainthread_message_queue_pop(MainThreadMessage* out) {
    if (!out || !g_queue_initialized) return false;
    memset(out, 0, sizeof(*out));

    void* item = NULL;
    if (!core_queue_mutex_pop(&g_queue, &item)) return false;

    MainThreadMessageNode* node = (MainThreadMessageNode*)item;
    if (!node) return false;

    *out = node->msg;
    free(node);

    stats_lock();
    g_popped++;
    stats_unlock();
    return true;
}

int mainthread_message_queue_drain(MainThreadMessage* out, int max_count) {
    if (!out || max_count <= 0) return 0;
    int n = 0;
    while (n < max_count && mainthread_message_queue_pop(&out[n])) {
        n++;
    }
    return n;
}

void mainthread_message_release(MainThreadMessage* msg) {
    release_owned(msg);
}

void mainthread_message_queue_snapshot(MainThreadMessageQueueStats* out) {
    if (!out) return;
    stats_lock();
    out->depth = g_queue_initialized ? (uint32_t)core_queue_mutex_size(&g_queue) : 0u;
    out->high_watermark = g_high_watermark;
    out->pushed = g_pushed;
    out->popped = g_popped;
    out->dropped_progress = g_dropped_progress;
    out->replaced_git_snapshots = g_replaced_git_snapshots;
    stats_unlock();
}
