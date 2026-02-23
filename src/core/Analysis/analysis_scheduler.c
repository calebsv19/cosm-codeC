#include "core/Analysis/analysis_scheduler.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>

#include "core/Analysis/analysis_job.h"
#include "core/Analysis/analysis_status.h"
#include "core/Watcher/file_watcher.h"

static SDL_mutex* g_scheduler_mutex = NULL;
static uint64_t g_next_run_id = 1;
static uint64_t g_active_run_id = 0;
static uint64_t g_last_completed_run_id = 0;
static unsigned int g_pending_reason_mask = 0;
static unsigned int g_active_reason_mask = 0;
static bool g_pending_force_full = false;
static bool g_active_force_full = false;
static bool g_running = false;
static unsigned int g_coalesced_requests = 0;

static void scheduler_lock(void) {
    if (g_scheduler_mutex) SDL_LockMutex(g_scheduler_mutex);
}

static void scheduler_unlock(void) {
    if (g_scheduler_mutex) SDL_UnlockMutex(g_scheduler_mutex);
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
        { ANALYSIS_REASON_LIBRARY_PANEL_REFRESH, "library_panel_refresh" }
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
    g_pending_reason_mask = 0;
    g_active_reason_mask = 0;
    g_pending_force_full = false;
    g_active_force_full = false;
    g_running = false;
    g_coalesced_requests = 0;
    scheduler_unlock();
}

void analysis_scheduler_request(AnalysisRefreshReason reason, bool force_full) {
    char reason_text[128];
    scheduler_lock();
    g_pending_reason_mask |= (unsigned int)reason;
    g_pending_force_full = g_pending_force_full || force_full;
    if (g_running) {
        g_coalesced_requests++;
    }
    unsigned int pending_mask = g_pending_reason_mask;
    bool pending_force_full = g_pending_force_full;
    bool running = g_running;
    unsigned int coalesced = g_coalesced_requests;
    scheduler_unlock();

    if (running && (reason & ANALYSIS_REASON_WORKSPACE_RELOAD)) {
        analysis_job_request_cancel();
    }

    printf("[AnalysisRun] queued reason=%s force_full=%d\n",
           analysis_scheduler_reason_mask_to_string(pending_mask, reason_text, sizeof(reason_text)),
           pending_force_full ? 1 : 0);
    if (running) printf("[AnalysisRun] coalesced pending=%u\n", coalesced);
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
    out->pending = (g_pending_reason_mask != 0);
    out->pending_force_full = g_pending_force_full;
    out->pending_reason_mask = g_pending_reason_mask;
    out->active_reason_mask = g_active_reason_mask;
    scheduler_unlock();
}

void analysis_scheduler_tick(const char* project_root, const char* build_args) {
    bool start_now = false;
    bool force_full = false;
    unsigned int reason_mask = 0;
    uint64_t run_id = 0;
    unsigned int coalesced = 0;

    scheduler_lock();
    bool worker_running = analysis_refresh_running();
    if (g_running && !worker_running) {
        char reason_text[128];
        printf("[AnalysisRun #%llu] completed reason=%s\n",
               (unsigned long long)g_active_run_id,
               analysis_scheduler_reason_mask_to_string(g_active_reason_mask,
                                                        reason_text,
                                                        sizeof(reason_text)));
        g_last_completed_run_id = g_active_run_id;
        g_active_run_id = 0;
        g_active_reason_mask = 0;
        g_active_force_full = false;
        g_running = false;
        // Suppress watcher-triggered refresh churn right after cache persistence.
        suppressInternalWatcherRefreshForMs(1200);
    }

    if (!g_running && g_pending_reason_mask != 0 && project_root && project_root[0]) {
        g_running = true;
        g_active_run_id = g_next_run_id++;
        g_active_reason_mask = g_pending_reason_mask;
        g_active_force_full = g_pending_force_full;

        run_id = g_active_run_id;
        reason_mask = g_active_reason_mask;
        force_full = g_active_force_full;
        coalesced = g_coalesced_requests;

        g_pending_reason_mask = 0;
        g_pending_force_full = false;
        g_coalesced_requests = 0;
        start_now = true;
    }
    scheduler_unlock();

    if (!start_now) return;

    if (force_full) {
        analysis_force_full_refresh_next_run();
    }
    // Suppress watcher-triggered refresh churn while analysis is actively persisting outputs.
    suppressInternalWatcherRefreshForMs(2500);

    char reason_text[128];
    printf("[AnalysisRun #%llu] started reason=%s force_full=%d coalesced=%u\n",
           (unsigned long long)run_id,
           analysis_scheduler_reason_mask_to_string(reason_mask, reason_text, sizeof(reason_text)),
           force_full ? 1 : 0,
           coalesced);
    start_async_workspace_analysis(project_root, build_args);
}
