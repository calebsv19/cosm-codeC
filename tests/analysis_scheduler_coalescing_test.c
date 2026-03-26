#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/Analysis/analysis_scheduler.h"

static bool g_worker_running = false;
static bool g_force_full_next = false;
static bool g_cancel_requested = false;
static uint64_t g_last_started_run_id = 0;
static char g_last_started_project_root[1024];
static char g_last_started_build_args[1024];
static unsigned int g_internal_watch_suppress_ms = 0;

bool analysis_refresh_running(void) {
    return g_worker_running;
}

void analysis_job_request_cancel(void) {
    g_cancel_requested = true;
}

void analysis_force_full_refresh_next_run(void) {
    g_force_full_next = true;
}

void start_async_workspace_analysis(const char* project_root, const char* build_args, uint64_t run_id) {
    g_worker_running = true;
    g_last_started_run_id = run_id;
    snprintf(g_last_started_project_root, sizeof(g_last_started_project_root), "%s", project_root ? project_root : "");
    g_last_started_project_root[sizeof(g_last_started_project_root) - 1] = '\0';
    snprintf(g_last_started_build_args, sizeof(g_last_started_build_args), "%s", build_args ? build_args : "");
    g_last_started_build_args[sizeof(g_last_started_build_args) - 1] = '\0';
}

void suppressInternalWatcherRefreshForMs(unsigned int durationMs) {
    g_internal_watch_suppress_ms = durationMs;
}

static void reset_stub_state(void) {
    g_worker_running = false;
    g_force_full_next = false;
    g_cancel_requested = false;
    g_last_started_run_id = 0;
    g_last_started_project_root[0] = '\0';
    g_last_started_build_args[0] = '\0';
    g_internal_watch_suppress_ms = 0;
}

static void test_same_key_latest_wins_coalescing(void) {
    reset_stub_state();
    analysis_scheduler_init();

    analysis_scheduler_request_key(ANALYSIS_JOB_KEY_SYMBOLS, ANALYSIS_REASON_MANUAL_REFRESH, false);
    analysis_scheduler_request_key(ANALYSIS_JOB_KEY_SYMBOLS, ANALYSIS_REASON_WATCHER_CHANGE, true);

    AnalysisSchedulerSnapshot snapshot;
    memset(&snapshot, 0, sizeof(snapshot));
    analysis_scheduler_snapshot(&snapshot);
    assert(snapshot.pending);
    assert(snapshot.pending_job_count == 1);
    assert(snapshot.pending_key_mask & (1u << ANALYSIS_JOB_KEY_SYMBOLS));
    assert(snapshot.pending_reason_mask & ANALYSIS_REASON_MANUAL_REFRESH);
    assert(snapshot.pending_reason_mask & ANALYSIS_REASON_WATCHER_CHANGE);
    assert(snapshot.pending_force_full);

    AnalysisSchedulerCounters counters;
    memset(&counters, 0, sizeof(counters));
    analysis_scheduler_counters_snapshot(&counters);
    assert(counters.jobs_scheduled == 2);
    assert(counters.jobs_coalesced_replaced == 1);
}

static void test_distinct_keys_queue_and_tick_order(void) {
    reset_stub_state();
    analysis_scheduler_init();

    analysis_scheduler_request_key(ANALYSIS_JOB_KEY_SYMBOLS, ANALYSIS_REASON_MANUAL_REFRESH, false);
    analysis_scheduler_request_key(ANALYSIS_JOB_KEY_DIAGNOSTICS, ANALYSIS_REASON_WATCHER_CHANGE, false);

    AnalysisSchedulerSnapshot snapshot;
    memset(&snapshot, 0, sizeof(snapshot));
    analysis_scheduler_snapshot(&snapshot);
    assert(snapshot.pending_job_count == 2);
    assert(snapshot.pending_key_mask & (1u << ANALYSIS_JOB_KEY_SYMBOLS));
    assert(snapshot.pending_key_mask & (1u << ANALYSIS_JOB_KEY_DIAGNOSTICS));

    analysis_scheduler_tick("/tmp/test_project", "-I/tmp");
    assert(g_worker_running);
    assert(g_last_started_run_id == 1);
    assert(strcmp(g_last_started_project_root, "/tmp/test_project") == 0);
    assert(strcmp(g_last_started_build_args, "-I/tmp") == 0);
    assert(g_internal_watch_suppress_ms == 2500);

    memset(&snapshot, 0, sizeof(snapshot));
    analysis_scheduler_snapshot(&snapshot);
    assert(snapshot.running);
    assert(snapshot.active_job_key == ANALYSIS_JOB_KEY_SYMBOLS);
    assert(snapshot.pending_job_count == 1);
    assert(snapshot.pending_key_mask & (1u << ANALYSIS_JOB_KEY_DIAGNOSTICS));

    // Simulate worker completion and ensure queued key starts next.
    g_worker_running = false;
    analysis_scheduler_tick("/tmp/test_project", "-I/tmp");
    assert(g_last_started_run_id == 2);

    memset(&snapshot, 0, sizeof(snapshot));
    analysis_scheduler_snapshot(&snapshot);
    assert(snapshot.running);
    assert(snapshot.active_job_key == ANALYSIS_JOB_KEY_DIAGNOSTICS);
}

static void test_index_lane_reason_normalizes_to_index_key(void) {
    reset_stub_state();
    analysis_scheduler_init();

    // Even if caller uses generic request(), index-lane reason should map to INDEX key.
    analysis_scheduler_request(ANALYSIS_REASON_LIBRARY_PANEL_REFRESH, true);

    AnalysisSchedulerSnapshot snapshot;
    memset(&snapshot, 0, sizeof(snapshot));
    analysis_scheduler_snapshot(&snapshot);
    assert(snapshot.pending);
    assert(snapshot.pending_job_count == 1);
    assert((snapshot.pending_key_mask & (1u << ANALYSIS_JOB_KEY_INDEX)) != 0u);
    assert((snapshot.pending_key_mask & (1u << ANALYSIS_JOB_KEY_WORKSPACE)) == 0u);

    analysis_scheduler_tick("/tmp/test_project", "-I/tmp");
    memset(&snapshot, 0, sizeof(snapshot));
    analysis_scheduler_snapshot(&snapshot);
    assert(snapshot.running);
    assert(snapshot.active_job_key == ANALYSIS_JOB_KEY_INDEX);
}

int main(void) {
    test_same_key_latest_wins_coalescing();
    test_distinct_keys_queue_and_tick_order();
    test_index_lane_reason_normalizes_to_index_key();
    puts("analysis_scheduler_coalescing_test: success");
    return 0;
}
