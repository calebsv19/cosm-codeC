#include "core/Analysis/analysis_scheduler.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>

#include "core/Analysis/analysis_job.h"
#include "core/Analysis/analysis_status.h"
#include "core/Watcher/file_watcher.h"

typedef struct PendingJobState {
    bool pending;
    bool force_full;
    unsigned int reason_mask;
    uint64_t enqueue_seq;
    unsigned int coalesced_replaced;
} PendingJobState;

static SDL_mutex* g_scheduler_mutex = NULL;
static uint64_t g_next_run_id = 1;
static uint64_t g_active_run_id = 0;
static uint64_t g_last_completed_run_id = 0;
static unsigned int g_active_reason_mask = 0;
static bool g_active_force_full = false;
static AnalysisJobKey g_active_job_key = ANALYSIS_JOB_KEY_NONE;
static bool g_running = false;

static PendingJobState g_pending_jobs[ANALYSIS_JOB_KEY_INDEX + 1];
static uint64_t g_next_enqueue_seq = 1;

static uint64_t g_jobs_scheduled = 0;
static uint64_t g_jobs_coalesced_replaced = 0;

static void scheduler_lock(void) {
    if (g_scheduler_mutex) SDL_LockMutex(g_scheduler_mutex);
}

static void scheduler_unlock(void) {
    if (g_scheduler_mutex) SDL_UnlockMutex(g_scheduler_mutex);
}

static bool is_valid_job_key(AnalysisJobKey key) {
    return key > ANALYSIS_JOB_KEY_NONE && key <= ANALYSIS_JOB_KEY_INDEX;
}

static bool reason_is_index_lane(unsigned int reason_mask) {
    // Current index-only lane reason; keep centralized for future expansion.
    return reason_mask == ANALYSIS_REASON_LIBRARY_PANEL_REFRESH;
}

static unsigned int pending_key_mask_unsafe(void) {
    unsigned int mask = 0;
    for (int i = (int)ANALYSIS_JOB_KEY_WORKSPACE; i <= (int)ANALYSIS_JOB_KEY_INDEX; ++i) {
        if (!g_pending_jobs[i].pending) continue;
        mask |= (1u << (unsigned int)i);
    }
    return mask;
}

static uint32_t pending_count_unsafe(void) {
    uint32_t count = 0;
    for (int i = (int)ANALYSIS_JOB_KEY_WORKSPACE; i <= (int)ANALYSIS_JOB_KEY_INDEX; ++i) {
        if (g_pending_jobs[i].pending) count++;
    }
    return count;
}

static unsigned int pending_reason_mask_unsafe(void) {
    unsigned int reason_mask = 0;
    for (int i = (int)ANALYSIS_JOB_KEY_WORKSPACE; i <= (int)ANALYSIS_JOB_KEY_INDEX; ++i) {
        if (!g_pending_jobs[i].pending) continue;
        reason_mask |= g_pending_jobs[i].reason_mask;
    }
    return reason_mask;
}

static bool pending_force_full_unsafe(void) {
    for (int i = (int)ANALYSIS_JOB_KEY_WORKSPACE; i <= (int)ANALYSIS_JOB_KEY_INDEX; ++i) {
        if (!g_pending_jobs[i].pending) continue;
        if (g_pending_jobs[i].force_full) return true;
    }
    return false;
}

static AnalysisJobKey select_next_pending_key_unsafe(void) {
    AnalysisJobKey selected = ANALYSIS_JOB_KEY_NONE;
    uint64_t selected_seq = 0;

    for (int i = (int)ANALYSIS_JOB_KEY_WORKSPACE; i <= (int)ANALYSIS_JOB_KEY_INDEX; ++i) {
        if (!g_pending_jobs[i].pending) continue;
        if (selected == ANALYSIS_JOB_KEY_NONE || g_pending_jobs[i].enqueue_seq < selected_seq) {
            selected = (AnalysisJobKey)i;
            selected_seq = g_pending_jobs[i].enqueue_seq;
        }
    }
    return selected;
}

const char* analysis_scheduler_key_to_string(AnalysisJobKey key) {
    switch (key) {
        case ANALYSIS_JOB_KEY_WORKSPACE: return "analysis:workspace";
        case ANALYSIS_JOB_KEY_SYMBOLS: return "symbols:workspace";
        case ANALYSIS_JOB_KEY_DIAGNOSTICS: return "diagnostics:workspace";
        case ANALYSIS_JOB_KEY_INDEX: return "index:workspace";
        default: return "none";
    }
}

const char* analysis_scheduler_reason_mask_to_string(unsigned int reason_mask,
                                                     char* buf,
                                                     int buf_size) {
    if (!buf || buf_size <= 0) return "";
    buf[0] = '\0';
    if (reason_mask == ANALYSIS_REASON_NONE) {
        snprintf(buf, (size_t)buf_size, "none");
        return buf;
    }

    bool first = true;
    struct {
        unsigned int bit;
        const char* label;
    } reasons[] = {
        { ANALYSIS_REASON_STARTUP, "startup" },
        { ANALYSIS_REASON_WORKSPACE_RELOAD, "workspace_reload" },
        { ANALYSIS_REASON_WATCHER_CHANGE, "watcher_change" },
        { ANALYSIS_REASON_MANUAL_REFRESH, "manual_refresh" },
        { ANALYSIS_REASON_PROJECT_MUTATION, "project_mutation" },
        { ANALYSIS_REASON_LIBRARY_PANEL_REFRESH, "library_panel_refresh" },
        { ANALYSIS_REASON_EDITOR_EDIT_TRANSACTION, "editor_edit_txn" }
    };

    for (size_t i = 0; i < sizeof(reasons) / sizeof(reasons[0]); ++i) {
        if ((reason_mask & reasons[i].bit) == 0) continue;
        if (!first) strncat(buf, "|", (size_t)(buf_size - (int)strlen(buf) - 1));
        strncat(buf, reasons[i].label, (size_t)(buf_size - (int)strlen(buf) - 1));
        first = false;
    }
    return buf;
}

void analysis_scheduler_init(void) {
    if (!g_scheduler_mutex) {
        g_scheduler_mutex = SDL_CreateMutex();
    }
    scheduler_lock();
    g_next_run_id = 1;
    g_active_run_id = 0;
    g_last_completed_run_id = 0;
    g_active_reason_mask = 0;
    g_active_force_full = false;
    g_active_job_key = ANALYSIS_JOB_KEY_NONE;
    g_running = false;
    memset(g_pending_jobs, 0, sizeof(g_pending_jobs));
    g_next_enqueue_seq = 1;
    g_jobs_scheduled = 0;
    g_jobs_coalesced_replaced = 0;
    scheduler_unlock();
}

void analysis_scheduler_request(AnalysisRefreshReason reason, bool force_full) {
    if (reason_is_index_lane((unsigned int)reason)) {
        analysis_scheduler_request_index(reason, force_full);
        return;
    }
    analysis_scheduler_request_key(ANALYSIS_JOB_KEY_WORKSPACE, reason, force_full);
}

void analysis_scheduler_request_index(AnalysisRefreshReason reason, bool force_full) {
    analysis_scheduler_request_key(ANALYSIS_JOB_KEY_INDEX, reason, force_full);
}

void analysis_scheduler_request_library_index_refresh(bool force_full) {
    analysis_scheduler_request_index(ANALYSIS_REASON_LIBRARY_PANEL_REFRESH, force_full);
}

void analysis_scheduler_request_key(AnalysisJobKey key,
                                    AnalysisRefreshReason reason,
                                    bool force_full) {
    if (!is_valid_job_key(key) || reason == ANALYSIS_REASON_NONE) return;
    if (reason_is_index_lane((unsigned int)reason) && key != ANALYSIS_JOB_KEY_INDEX) {
#ifndef NDEBUG
        fprintf(stderr,
                "[AnalysisSchedulerGuard] remapping index-lane reason to index key (requested=%s)\n",
                analysis_scheduler_key_to_string(key));
#endif
        key = ANALYSIS_JOB_KEY_INDEX;
    }

    char reason_text[128];
    bool running = false;
    bool active_key_match = false;
    bool should_cancel = false;
    unsigned int pending_mask = 0;
    unsigned int slot_coalesced = 0;
    unsigned int slot_reason_mask = 0;
    bool slot_force_full = false;

    scheduler_lock();
    PendingJobState* slot = &g_pending_jobs[(int)key];
    bool replaced_pending = slot->pending;
    if (!slot->pending) {
        slot->pending = true;
        slot->enqueue_seq = g_next_enqueue_seq++;
        slot->reason_mask = ANALYSIS_REASON_NONE;
        slot->force_full = false;
        slot->coalesced_replaced = 0;
    }

    slot->reason_mask |= (unsigned int)reason;
    slot->force_full = slot->force_full || force_full;
    g_jobs_scheduled++;

    running = g_running;
    active_key_match = running && (g_active_job_key == key);
    if (replaced_pending || active_key_match) {
        slot->coalesced_replaced++;
        g_jobs_coalesced_replaced++;
    }
    if (active_key_match && (reason & ANALYSIS_REASON_WORKSPACE_RELOAD)) {
        should_cancel = true;
    }

    pending_mask = pending_key_mask_unsafe();
    slot_coalesced = slot->coalesced_replaced;
    slot_reason_mask = slot->reason_mask;
    slot_force_full = slot->force_full;
    scheduler_unlock();

    if (should_cancel) {
        analysis_job_request_cancel();
    }

    printf("[AnalysisRun] queued key=%s reason=%s force_full=%d pending_keys=0x%x\n",
           analysis_scheduler_key_to_string(key),
           analysis_scheduler_reason_mask_to_string(slot_reason_mask, reason_text, sizeof(reason_text)),
           slot_force_full ? 1 : 0,
           pending_mask);
    if (slot_coalesced > 0) {
        printf("[AnalysisRun] key=%s coalesced=%u\n",
               analysis_scheduler_key_to_string(key),
               slot_coalesced);
    }
}

bool analysis_scheduler_running(void) {
    scheduler_lock();
    bool running = g_running;
    scheduler_unlock();
    return running;
}

void analysis_scheduler_snapshot(AnalysisSchedulerSnapshot* out) {
    if (!out) return;
    scheduler_lock();
    out->active_run_id = g_active_run_id;
    out->last_completed_run_id = g_last_completed_run_id;
    out->running = g_running;
    out->pending_job_count = pending_count_unsafe();
    out->pending = (out->pending_job_count > 0);
    out->pending_force_full = pending_force_full_unsafe();
    out->pending_reason_mask = pending_reason_mask_unsafe();
    out->pending_key_mask = pending_key_mask_unsafe();
    out->active_reason_mask = g_active_reason_mask;
    out->active_job_key = g_active_job_key;
    scheduler_unlock();
}

void analysis_scheduler_counters_snapshot(AnalysisSchedulerCounters* out) {
    if (!out) return;
    scheduler_lock();
    out->jobs_scheduled = g_jobs_scheduled;
    out->jobs_coalesced_replaced = g_jobs_coalesced_replaced;
    scheduler_unlock();
}

void analysis_scheduler_tick(const char* project_root, const char* build_args) {
    bool start_now = false;
    bool force_full = false;
    unsigned int reason_mask = 0;
    uint64_t run_id = 0;
    unsigned int coalesced = 0;
    AnalysisJobKey job_key = ANALYSIS_JOB_KEY_NONE;

    scheduler_lock();
    bool worker_running = analysis_refresh_running();
    if (g_running && !worker_running) {
        char reason_text[128];
        printf("[AnalysisRun #%llu] completed key=%s reason=%s\n",
               (unsigned long long)g_active_run_id,
               analysis_scheduler_key_to_string(g_active_job_key),
               analysis_scheduler_reason_mask_to_string(g_active_reason_mask,
                                                        reason_text,
                                                        sizeof(reason_text)));
        g_last_completed_run_id = g_active_run_id;
        g_active_run_id = 0;
        g_active_reason_mask = 0;
        g_active_force_full = false;
        g_active_job_key = ANALYSIS_JOB_KEY_NONE;
        g_running = false;
        // Suppress watcher-triggered refresh churn right after cache persistence.
        suppressInternalWatcherRefreshForMs(1200);
    }

    if (!g_running && project_root && project_root[0]) {
        job_key = select_next_pending_key_unsafe();
        if (job_key != ANALYSIS_JOB_KEY_NONE) {
            PendingJobState* slot = &g_pending_jobs[(int)job_key];
            g_running = true;
            g_active_run_id = g_next_run_id++;
            g_active_reason_mask = slot->reason_mask;
            g_active_force_full = slot->force_full;
            g_active_job_key = job_key;

            run_id = g_active_run_id;
            reason_mask = g_active_reason_mask;
            force_full = g_active_force_full;
            coalesced = slot->coalesced_replaced;

            memset(slot, 0, sizeof(*slot));
            start_now = true;
        }
    }
    scheduler_unlock();

    if (!start_now) return;

    if (force_full) {
        analysis_force_full_refresh_next_run();
    }
    // Suppress watcher-triggered refresh churn while analysis is actively persisting outputs.
    suppressInternalWatcherRefreshForMs(2500);

    char reason_text[128];
    printf("[AnalysisRun #%llu] started key=%s reason=%s force_full=%d coalesced=%u\n",
           (unsigned long long)run_id,
           analysis_scheduler_key_to_string(job_key),
           analysis_scheduler_reason_mask_to_string(reason_mask, reason_text, sizeof(reason_text)),
           force_full ? 1 : 0,
           coalesced);
    start_async_workspace_analysis(project_root, build_args, run_id);
}
