#include "core/LoopResults/completed_results_queue.h"

#include <SDL2/SDL.h>
#include <stdlib.h>
#include <string.h>

#include "core_queue.h"

typedef struct CompletedResultNode {
    CompletedResult result;
} CompletedResultNode;

enum { COMPLETED_RESULTS_QUEUE_CAPACITY = 512 };

static CoreQueueMutex g_analysis_queue;
static CoreQueueMutex g_symbols_queue;
static CoreQueueMutex g_diagnostics_queue;
static void* g_analysis_backing[COMPLETED_RESULTS_QUEUE_CAPACITY];
static void* g_symbols_backing[COMPLETED_RESULTS_QUEUE_CAPACITY];
static void* g_diagnostics_backing[COMPLETED_RESULTS_QUEUE_CAPACITY];
static bool g_initialized = false;
static SDL_mutex* g_stats_mutex = NULL;

static uint64_t g_next_seq = 1;
static uint32_t g_high_watermark = 0;
static uint64_t g_pushed = 0;
static uint64_t g_popped = 0;
static uint64_t g_results_applied = 0;
static uint64_t g_results_stale_dropped = 0;
static bool g_head_analysis_valid = false;
static bool g_head_symbols_valid = false;
static bool g_head_diagnostics_valid = false;
static CompletedResult g_head_analysis;
static CompletedResult g_head_symbols;
static CompletedResult g_head_diagnostics;

static void stats_lock(void) {
    if (g_stats_mutex) SDL_LockMutex(g_stats_mutex);
}

static void stats_unlock(void) {
    if (g_stats_mutex) SDL_UnlockMutex(g_stats_mutex);
}

static uint32_t total_depth_unsafe(void) {
    if (!g_initialized) return 0u;
    return (uint32_t)(core_queue_mutex_size(&g_analysis_queue) +
                      core_queue_mutex_size(&g_symbols_queue) +
                      core_queue_mutex_size(&g_diagnostics_queue));
}

static CoreQueueMutex* queue_for_subsystem(CompletedResultSubsystem subsystem) {
    switch (subsystem) {
        case COMPLETED_SUBSYSTEM_ANALYSIS: return &g_analysis_queue;
        case COMPLETED_SUBSYSTEM_SYMBOLS: return &g_symbols_queue;
        case COMPLETED_SUBSYSTEM_DIAGNOSTICS: return &g_diagnostics_queue;
        default: return NULL;
    }
}

static bool head_valid_for_subsystem(CompletedResultSubsystem subsystem) {
    switch (subsystem) {
        case COMPLETED_SUBSYSTEM_ANALYSIS: return g_head_analysis_valid;
        case COMPLETED_SUBSYSTEM_SYMBOLS: return g_head_symbols_valid;
        case COMPLETED_SUBSYSTEM_DIAGNOSTICS: return g_head_diagnostics_valid;
        default: return false;
    }
}

static CompletedResult* head_for_subsystem(CompletedResultSubsystem subsystem) {
    switch (subsystem) {
        case COMPLETED_SUBSYSTEM_ANALYSIS: return &g_head_analysis;
        case COMPLETED_SUBSYSTEM_SYMBOLS: return &g_head_symbols;
        case COMPLETED_SUBSYSTEM_DIAGNOSTICS: return &g_head_diagnostics;
        default: return NULL;
    }
}

static void set_head_valid_for_subsystem(CompletedResultSubsystem subsystem, bool valid) {
    switch (subsystem) {
        case COMPLETED_SUBSYSTEM_ANALYSIS:
            g_head_analysis_valid = valid;
            break;
        case COMPLETED_SUBSYSTEM_SYMBOLS:
            g_head_symbols_valid = valid;
            break;
        case COMPLETED_SUBSYSTEM_DIAGNOSTICS:
            g_head_diagnostics_valid = valid;
            break;
        default:
            break;
    }
}

void completed_results_queue_release(CompletedResult* result) {
    if (!result) return;
    if (result->owned_ptr && result->free_owned_ptr) {
        result->free_owned_ptr(result->owned_ptr);
    }
    memset(result, 0, sizeof(*result));
}

static void clear_head_result(CompletedResultSubsystem subsystem) {
    CompletedResult* head = head_for_subsystem(subsystem);
    if (head && head_valid_for_subsystem(subsystem)) {
        completed_results_queue_release(head);
        set_head_valid_for_subsystem(subsystem, false);
    }
}

static void clear_all_heads(void) {
    clear_head_result(COMPLETED_SUBSYSTEM_ANALYSIS);
    clear_head_result(COMPLETED_SUBSYSTEM_SYMBOLS);
    clear_head_result(COMPLETED_SUBSYSTEM_DIAGNOSTICS);
}

static bool refill_head_from_queue(CompletedResultSubsystem subsystem) {
    if (!g_initialized) return false;
    if (head_valid_for_subsystem(subsystem)) return true;
    CoreQueueMutex* queue = queue_for_subsystem(subsystem);
    CompletedResult* head = head_for_subsystem(subsystem);
    if (!queue || !head) return false;

    void* item = NULL;
    if (!core_queue_mutex_pop(queue, &item)) return false;
    CompletedResultNode* node = (CompletedResultNode*)item;
    if (!node) return false;
    *head = node->result;
    free(node);
    set_head_valid_for_subsystem(subsystem, true);
    return true;
}

void completed_results_queue_init(void) {
    if (!g_stats_mutex) g_stats_mutex = SDL_CreateMutex();
    if (!g_initialized) {
        bool ok_analysis = core_queue_mutex_init_ex(&g_analysis_queue,
                                                    g_analysis_backing,
                                                    COMPLETED_RESULTS_QUEUE_CAPACITY,
                                                    CORE_QUEUE_OVERFLOW_REJECT);
        bool ok_symbols = core_queue_mutex_init_ex(&g_symbols_queue,
                                                   g_symbols_backing,
                                                   COMPLETED_RESULTS_QUEUE_CAPACITY,
                                                   CORE_QUEUE_OVERFLOW_REJECT);
        bool ok_diagnostics = core_queue_mutex_init_ex(&g_diagnostics_queue,
                                                       g_diagnostics_backing,
                                                       COMPLETED_RESULTS_QUEUE_CAPACITY,
                                                       CORE_QUEUE_OVERFLOW_REJECT);
        g_initialized = ok_analysis && ok_symbols && ok_diagnostics;
        if (!(ok_analysis && ok_symbols && ok_diagnostics)) {
            if (ok_analysis) core_queue_mutex_destroy(&g_analysis_queue);
            if (ok_symbols) core_queue_mutex_destroy(&g_symbols_queue);
            if (ok_diagnostics) core_queue_mutex_destroy(&g_diagnostics_queue);
        }
    }
    completed_results_queue_reset();
}

void completed_results_queue_shutdown(void) {
    completed_results_queue_reset();
    if (g_initialized) {
        core_queue_mutex_destroy(&g_analysis_queue);
        core_queue_mutex_destroy(&g_symbols_queue);
        core_queue_mutex_destroy(&g_diagnostics_queue);
        g_initialized = false;
    }
    if (g_stats_mutex) {
        SDL_DestroyMutex(g_stats_mutex);
        g_stats_mutex = NULL;
    }
}

void completed_results_queue_reset(void) {
    clear_all_heads();
    if (g_initialized) {
        void* item = NULL;
        CompletedResultNode* node = NULL;
        while (core_queue_mutex_pop(&g_analysis_queue, &item)) {
            node = (CompletedResultNode*)item;
            if (node) completed_results_queue_release(&node->result);
            free(node);
        }
        while (core_queue_mutex_pop(&g_symbols_queue, &item)) {
            node = (CompletedResultNode*)item;
            if (node) completed_results_queue_release(&node->result);
            free(node);
        }
        while (core_queue_mutex_pop(&g_diagnostics_queue, &item)) {
            node = (CompletedResultNode*)item;
            if (node) completed_results_queue_release(&node->result);
            free(node);
        }
    }

    stats_lock();
    g_next_seq = 1;
    g_high_watermark = 0;
    g_pushed = 0;
    g_popped = 0;
    g_results_applied = 0;
    g_results_stale_dropped = 0;
    stats_unlock();
}

bool completed_results_queue_push(const CompletedResult* result) {
    if (!result || result->kind == COMPLETED_RESULT_NONE || !g_initialized) return false;
    CoreQueueMutex* queue = queue_for_subsystem(result->subsystem);
    if (!queue) return false;

    CompletedResultNode* node = (CompletedResultNode*)calloc(1, sizeof(*node));
    if (!node) return false;
    node->result = *result;

    stats_lock();
    node->result.seq = g_next_seq++;
    stats_unlock();

    if (!core_queue_mutex_push(queue, node)) {
        completed_results_queue_release(&node->result);
        free(node);
        return false;
    }

    stats_lock();
    g_pushed++;
    uint32_t depth = total_depth_unsafe();
    if (depth > g_high_watermark) g_high_watermark = depth;
    stats_unlock();
    return true;
}

bool completed_results_queue_pop(CompletedResultSubsystem subsystem, CompletedResult* out) {
    if (!out || !g_initialized) return false;
    memset(out, 0, sizeof(*out));
    if (subsystem != COMPLETED_SUBSYSTEM_ANALYSIS &&
        subsystem != COMPLETED_SUBSYSTEM_SYMBOLS &&
        subsystem != COMPLETED_SUBSYSTEM_DIAGNOSTICS) {
        return false;
    }
    if (!refill_head_from_queue(subsystem)) return false;
    CompletedResult* head = head_for_subsystem(subsystem);
    if (!head) return false;
    *out = *head;
    memset(head, 0, sizeof(*head));
    set_head_valid_for_subsystem(subsystem, false);

    stats_lock();
    g_popped++;
    stats_unlock();
    return true;
}

bool completed_results_queue_pop_any(CompletedResult* out) {
    if (!out || !g_initialized) return false;
    memset(out, 0, sizeof(*out));

    refill_head_from_queue(COMPLETED_SUBSYSTEM_ANALYSIS);
    refill_head_from_queue(COMPLETED_SUBSYSTEM_SYMBOLS);
    refill_head_from_queue(COMPLETED_SUBSYSTEM_DIAGNOSTICS);

    bool have_choice = false;
    CompletedResultSubsystem chosen_subsystem = COMPLETED_SUBSYSTEM_NONE;
    uint64_t chosen_seq = 0;

    if (g_head_analysis_valid) {
        chosen_subsystem = COMPLETED_SUBSYSTEM_ANALYSIS;
        chosen_seq = g_head_analysis.seq;
        have_choice = true;
    }
    if (g_head_symbols_valid && (!have_choice || g_head_symbols.seq < chosen_seq)) {
        chosen_subsystem = COMPLETED_SUBSYSTEM_SYMBOLS;
        chosen_seq = g_head_symbols.seq;
        have_choice = true;
    }
    if (g_head_diagnostics_valid && (!have_choice || g_head_diagnostics.seq < chosen_seq)) {
        chosen_subsystem = COMPLETED_SUBSYSTEM_DIAGNOSTICS;
        have_choice = true;
    }

    if (!have_choice) return false;
    return completed_results_queue_pop(chosen_subsystem, out);
}

void completed_results_queue_note_applied(void) {
    stats_lock();
    g_results_applied++;
    stats_unlock();
}

void completed_results_queue_note_stale_dropped(void) {
    stats_lock();
    g_results_stale_dropped++;
    stats_unlock();
}

void completed_results_queue_snapshot(CompletedResultsQueueStats* out) {
    if (!out) return;
    stats_lock();
    out->analysis_depth = g_initialized ? (uint32_t)core_queue_mutex_size(&g_analysis_queue) : 0u;
    out->symbols_depth = g_initialized ? (uint32_t)core_queue_mutex_size(&g_symbols_queue) : 0u;
    out->diagnostics_depth = g_initialized ? (uint32_t)core_queue_mutex_size(&g_diagnostics_queue) : 0u;
    out->total_depth = out->analysis_depth + out->symbols_depth + out->diagnostics_depth;
    out->high_watermark = g_high_watermark;
    out->pushed = g_pushed;
    out->popped = g_popped;
    out->results_applied = g_results_applied;
    out->results_stale_dropped = g_results_stale_dropped;
    stats_unlock();
}
