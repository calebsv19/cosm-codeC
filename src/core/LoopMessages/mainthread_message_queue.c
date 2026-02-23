#include "core/LoopMessages/mainthread_message_queue.h"

#include <SDL2/SDL.h>
#include <stdlib.h>
#include <string.h>

typedef struct MainThreadMessageNode {
    MainThreadMessage msg;
    struct MainThreadMessageNode* next;
} MainThreadMessageNode;

static SDL_mutex* g_msg_mutex = NULL;
static MainThreadMessageNode* g_head = NULL;
static MainThreadMessageNode* g_tail = NULL;
static uint64_t g_next_seq = 1;
static uint32_t g_depth = 0;
static uint32_t g_high_watermark = 0;
static uint64_t g_pushed = 0;
static uint64_t g_popped = 0;
static uint64_t g_dropped_progress = 0;
static uint64_t g_replaced_git_snapshots = 0;
static Uint32 g_last_progress_push_ms = 0;

static void queue_lock(void) {
    if (g_msg_mutex) SDL_LockMutex(g_msg_mutex);
}

static void queue_unlock(void) {
    if (g_msg_mutex) SDL_UnlockMutex(g_msg_mutex);
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
    if (!g_msg_mutex) g_msg_mutex = SDL_CreateMutex();
    mainthread_message_queue_reset();
}

void mainthread_message_queue_shutdown(void) {
    mainthread_message_queue_reset();
    if (g_msg_mutex) {
        SDL_DestroyMutex(g_msg_mutex);
        g_msg_mutex = NULL;
    }
}

void mainthread_message_queue_reset(void) {
    queue_lock();
    MainThreadMessageNode* n = g_head;
    while (n) {
        MainThreadMessageNode* next = n->next;
        release_owned(&n->msg);
        free(n);
        n = next;
    }
    g_head = NULL;
    g_tail = NULL;
    g_next_seq = 1;
    g_depth = 0;
    g_high_watermark = 0;
    g_pushed = 0;
    g_popped = 0;
    g_dropped_progress = 0;
    g_replaced_git_snapshots = 0;
    g_last_progress_push_ms = 0;
    queue_unlock();
}

static bool push_internal(const MainThreadMessage* msg, bool coalesce_git) {
    if (!msg || msg->type == MAINTHREAD_MSG_NONE) return false;
    MainThreadMessageNode* node = (MainThreadMessageNode*)calloc(1, sizeof(*node));
    if (!node) return false;
    node->msg = *msg;

    queue_lock();
    node->msg.seq = g_next_seq++;

    if (coalesce_git && node->msg.type == MAINTHREAD_MSG_GIT_SNAPSHOT) {
        MainThreadMessageNode* prev = NULL;
        MainThreadMessageNode* cur = g_head;
        while (cur) {
            if (cur->msg.type == MAINTHREAD_MSG_GIT_SNAPSHOT) {
                if (prev) prev->next = cur->next;
                else g_head = cur->next;
                if (g_tail == cur) g_tail = prev;
                release_owned(&cur->msg);
                free(cur);
                if (g_depth > 0) g_depth--;
                g_replaced_git_snapshots++;
                break;
            }
            prev = cur;
            cur = cur->next;
        }
    }

    if (g_tail) g_tail->next = node;
    else g_head = node;
    g_tail = node;
    g_depth++;
    if (g_depth > g_high_watermark) g_high_watermark = g_depth;
    g_pushed++;
    queue_unlock();
    return true;
}

bool mainthread_message_queue_push(const MainThreadMessage* msg) {
    return push_internal(msg, true);
}

bool mainthread_message_queue_push_progress_throttled(const MainThreadMessage* msg,
                                                      uint32_t min_interval_ms) {
    if (!msg || msg->type != MAINTHREAD_MSG_PROGRESS) return false;
    Uint32 now = SDL_GetTicks();
    if (min_interval_ms > 0 && (now - g_last_progress_push_ms) < min_interval_ms) {
        queue_lock();
        g_dropped_progress++;
        queue_unlock();
        return false;
    }
    g_last_progress_push_ms = now;
    return push_internal(msg, true);
}

bool mainthread_message_queue_pop(MainThreadMessage* out) {
    if (!out) return false;
    memset(out, 0, sizeof(*out));

    queue_lock();
    MainThreadMessageNode* node = g_head;
    if (!node) {
        queue_unlock();
        return false;
    }
    g_head = node->next;
    if (!g_head) g_tail = NULL;
    if (g_depth > 0) g_depth--;
    g_popped++;
    queue_unlock();

    *out = node->msg;
    free(node);
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
    queue_lock();
    out->depth = g_depth;
    out->high_watermark = g_high_watermark;
    out->pushed = g_pushed;
    out->popped = g_popped;
    out->dropped_progress = g_dropped_progress;
    out->replaced_git_snapshots = g_replaced_git_snapshots;
    queue_unlock();
}

