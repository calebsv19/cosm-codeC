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
static char g_last_started_file_hint[1024];
static size_t g_last_started_file_hint_count = 0;
static char g_last_started_live_file[1024];
static char g_last_started_live_contents[1024];
static size_t g_last_started_live_length = 0;
static uint64_t g_last_started_live_revision = 0;
static unsigned int g_internal_watch_suppress_ms = 0;

bool analysis_refresh_running(void) {
    return g_worker_running;
}

void start_async_workspace_analysis_with_file_hints(const char* project_root,
                                                    const char* build_args,
                                                    uint64_t run_id,
                                                    const char* const* file_hints,
                                                    size_t file_hint_count);
void start_async_live_buffer_analysis(const char* project_root,
                                      const char* build_args,
                                      uint64_t run_id,
                                      const char* file_path,
                                      const char* contents,
                                      size_t content_length,
                                      uint64_t document_revision);

void analysis_job_request_cancel(void) {
    g_cancel_requested = true;
}

void analysis_force_full_refresh_next_run(void) {
    g_force_full_next = true;
}

void start_async_workspace_analysis(const char* project_root, const char* build_args, uint64_t run_id) {
    start_async_workspace_analysis_with_file_hints(project_root, build_args, run_id, NULL, 0);
}

void start_async_workspace_analysis_with_file_hints(const char* project_root,
                                                    const char* build_args,
                                                    uint64_t run_id,
                                                    const char* const* file_hints,
                                                    size_t file_hint_count) {
    g_worker_running = true;
    g_last_started_run_id = run_id;
    snprintf(g_last_started_project_root, sizeof(g_last_started_project_root), "%s", project_root ? project_root : "");
    g_last_started_project_root[sizeof(g_last_started_project_root) - 1] = '\0';
    snprintf(g_last_started_build_args, sizeof(g_last_started_build_args), "%s", build_args ? build_args : "");
    g_last_started_build_args[sizeof(g_last_started_build_args) - 1] = '\0';
    g_last_started_file_hint_count = file_hint_count;
    snprintf(g_last_started_file_hint,
             sizeof(g_last_started_file_hint),
             "%s",
             (file_hints && file_hint_count > 0 && file_hints[0]) ? file_hints[0] : "");
    g_last_started_file_hint[sizeof(g_last_started_file_hint) - 1] = '\0';
}

void start_async_live_buffer_analysis(const char* project_root,
                                      const char* build_args,
                                      uint64_t run_id,
                                      const char* file_path,
                                      const char* contents,
                                      size_t content_length,
                                      uint64_t document_revision) {
    g_worker_running = true;
    g_last_started_run_id = run_id;
    snprintf(g_last_started_project_root, sizeof(g_last_started_project_root), "%s", project_root ? project_root : "");
    g_last_started_project_root[sizeof(g_last_started_project_root) - 1] = '\0';
    snprintf(g_last_started_build_args, sizeof(g_last_started_build_args), "%s", build_args ? build_args : "");
    g_last_started_build_args[sizeof(g_last_started_build_args) - 1] = '\0';
    snprintf(g_last_started_live_file, sizeof(g_last_started_live_file), "%s", file_path ? file_path : "");
    g_last_started_live_file[sizeof(g_last_started_live_file) - 1] = '\0';
    g_last_started_live_length = content_length;
    size_t copy_len = content_length < sizeof(g_last_started_live_contents) - 1
        ? content_length
        : sizeof(g_last_started_live_contents) - 1;
    if (contents && copy_len > 0) {
        memcpy(g_last_started_live_contents, contents, copy_len);
    }
    g_last_started_live_contents[copy_len] = '\0';
    g_last_started_live_revision = document_revision;
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
    g_last_started_file_hint[0] = '\0';
    g_last_started_file_hint_count = 0;
    g_last_started_live_file[0] = '\0';
    g_last_started_live_contents[0] = '\0';
    g_last_started_live_length = 0;
    g_last_started_live_revision = 0;
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

static void test_editor_save_reason_is_reported(void) {
    char reason_text[128];
    const char* text = analysis_scheduler_reason_mask_to_string(ANALYSIS_REASON_EDITOR_SAVE,
                                                                reason_text,
                                                                sizeof(reason_text));
    assert(strcmp(text, "editor_save") == 0);

    text = analysis_scheduler_reason_mask_to_string(ANALYSIS_REASON_EDITOR_LIVE_BUFFER,
                                                    reason_text,
                                                    sizeof(reason_text));
    assert(strcmp(text, "editor_live_buffer") == 0);
}

static void test_file_hint_reaches_worker_start(void) {
    reset_stub_state();
    analysis_scheduler_init();

    analysis_scheduler_request_file("/tmp/test_project/src/main.c",
                                    ANALYSIS_REASON_EDITOR_SAVE,
                                    false);

    AnalysisSchedulerSnapshot snapshot;
    memset(&snapshot, 0, sizeof(snapshot));
    analysis_scheduler_snapshot(&snapshot);
    assert(snapshot.pending);
    assert(snapshot.pending_job_count == 1);
    assert(snapshot.pending_key_mask & (1u << ANALYSIS_JOB_KEY_DIAGNOSTICS));
    assert(snapshot.pending_reason_mask & ANALYSIS_REASON_EDITOR_SAVE);
    assert(snapshot.pending_editor_save_diagnostics);

    analysis_scheduler_tick("/tmp/test_project", "-I/tmp");
    assert(g_worker_running);
    assert(g_last_started_run_id == 1);
    assert(g_last_started_file_hint_count == 1);
    assert(strcmp(g_last_started_file_hint, "/tmp/test_project/src/main.c") == 0);
}

static void test_editor_save_diagnostics_preempts_pending_symbols(void) {
    reset_stub_state();
    analysis_scheduler_init();

    analysis_scheduler_request_key(ANALYSIS_JOB_KEY_SYMBOLS,
                                   ANALYSIS_REASON_EDITOR_EDIT_TRANSACTION,
                                   false);
    analysis_scheduler_request_file("/tmp/test_project/src/saved.c",
                                    ANALYSIS_REASON_EDITOR_SAVE,
                                    false);

    AnalysisSchedulerSnapshot snapshot;
    memset(&snapshot, 0, sizeof(snapshot));
    analysis_scheduler_snapshot(&snapshot);
    assert(snapshot.pending_job_count == 2);
    assert(snapshot.pending_key_mask & (1u << ANALYSIS_JOB_KEY_SYMBOLS));
    assert(snapshot.pending_key_mask & (1u << ANALYSIS_JOB_KEY_DIAGNOSTICS));

    analysis_scheduler_tick("/tmp/test_project", "-I/tmp");
    assert(g_worker_running);
    assert(g_last_started_run_id == 1);
    assert(g_last_started_file_hint_count == 1);
    assert(strcmp(g_last_started_file_hint, "/tmp/test_project/src/saved.c") == 0);

    memset(&snapshot, 0, sizeof(snapshot));
    analysis_scheduler_snapshot(&snapshot);
    assert(snapshot.running);
    assert(snapshot.active_job_key == ANALYSIS_JOB_KEY_DIAGNOSTICS);
    assert(snapshot.active_reason_mask & ANALYSIS_REASON_EDITOR_SAVE);
    assert(snapshot.pending_key_mask & (1u << ANALYSIS_JOB_KEY_SYMBOLS));
}

static void test_editor_save_cancels_active_startup_workspace_run(void) {
    reset_stub_state();
    analysis_scheduler_init();

    analysis_scheduler_request_key(ANALYSIS_JOB_KEY_WORKSPACE,
                                   ANALYSIS_REASON_STARTUP,
                                   false);
    analysis_scheduler_tick("/tmp/test_project", "-I/tmp");
    assert(g_worker_running);
    assert(!g_cancel_requested);

    AnalysisSchedulerSnapshot snapshot;
    memset(&snapshot, 0, sizeof(snapshot));
    analysis_scheduler_snapshot(&snapshot);
    assert(snapshot.running);
    assert(snapshot.active_job_key == ANALYSIS_JOB_KEY_WORKSPACE);
    assert(snapshot.active_reason_mask & ANALYSIS_REASON_STARTUP);

    analysis_scheduler_request_file("/tmp/test_project/src/saved.c",
                                    ANALYSIS_REASON_EDITOR_SAVE,
                                    false);
    assert(g_cancel_requested);

    memset(&snapshot, 0, sizeof(snapshot));
    analysis_scheduler_snapshot(&snapshot);
    assert(snapshot.running);
    assert(snapshot.active_job_key == ANALYSIS_JOB_KEY_WORKSPACE);
    assert(snapshot.pending_key_mask & (1u << ANALYSIS_JOB_KEY_DIAGNOSTICS));
    assert(snapshot.pending_reason_mask & ANALYSIS_REASON_EDITOR_SAVE);
    assert(snapshot.pending_editor_save_diagnostics);

    g_worker_running = false;
    g_cancel_requested = false;
    analysis_scheduler_tick("/tmp/test_project", "-I/tmp");
    analysis_scheduler_tick("/tmp/test_project", "-I/tmp");
    assert(g_worker_running);
    assert(g_last_started_run_id == 2);
    assert(g_last_started_file_hint_count == 1);
    assert(strcmp(g_last_started_file_hint, "/tmp/test_project/src/saved.c") == 0);

    memset(&snapshot, 0, sizeof(snapshot));
    analysis_scheduler_snapshot(&snapshot);
    assert(snapshot.running);
    assert(snapshot.active_job_key == ANALYSIS_JOB_KEY_DIAGNOSTICS);
    assert(snapshot.active_reason_mask & ANALYSIS_REASON_EDITOR_SAVE);
    assert(!snapshot.pending_editor_save_diagnostics);
}

static void test_live_buffer_snapshot_reaches_worker_start(void) {
    reset_stub_state();
    analysis_scheduler_init();

    const char* contents = "int main(void) { return broken }\n";
    analysis_scheduler_request_live_buffer("/tmp/test_project/src/live.c",
                                           contents,
                                           strlen(contents),
                                           42u,
                                           ANALYSIS_REASON_EDITOR_LIVE_BUFFER,
                                           false);

    AnalysisSchedulerSnapshot snapshot;
    memset(&snapshot, 0, sizeof(snapshot));
    analysis_scheduler_snapshot(&snapshot);
    assert(snapshot.pending);
    assert(snapshot.pending_key_mask & (1u << ANALYSIS_JOB_KEY_LIVE_DIAGNOSTICS));
    assert(snapshot.pending_reason_mask & ANALYSIS_REASON_EDITOR_LIVE_BUFFER);

    analysis_scheduler_tick("/tmp/test_project", "-I/tmp");
    assert(g_worker_running);
    assert(g_last_started_run_id == 1);
    assert(strcmp(g_last_started_live_file, "/tmp/test_project/src/live.c") == 0);
    assert(strcmp(g_last_started_live_contents, contents) == 0);
    assert(g_last_started_live_length == strlen(contents));
    assert(g_last_started_live_revision == 42u);

    memset(&snapshot, 0, sizeof(snapshot));
    analysis_scheduler_snapshot(&snapshot);
    assert(snapshot.running);
    assert(snapshot.active_job_key == ANALYSIS_JOB_KEY_LIVE_DIAGNOSTICS);
}

static void test_live_buffer_latest_snapshot_wins(void) {
    reset_stub_state();
    analysis_scheduler_init();

    const char* first = "int a;\n";
    const char* second = "int b;\n";
    analysis_scheduler_request_live_buffer("/tmp/test_project/src/live.c",
                                           first,
                                           strlen(first),
                                           10u,
                                           ANALYSIS_REASON_EDITOR_LIVE_BUFFER,
                                           false);
    analysis_scheduler_request_live_buffer("/tmp/test_project/src/live.c",
                                           second,
                                           strlen(second),
                                           11u,
                                           ANALYSIS_REASON_EDITOR_LIVE_BUFFER,
                                           false);

    AnalysisSchedulerSnapshot snapshot;
    memset(&snapshot, 0, sizeof(snapshot));
    analysis_scheduler_snapshot(&snapshot);
    assert(snapshot.pending_job_count == 1);

    AnalysisSchedulerCounters counters;
    memset(&counters, 0, sizeof(counters));
    analysis_scheduler_counters_snapshot(&counters);
    assert(counters.jobs_scheduled == 2);
    assert(counters.jobs_coalesced_replaced == 1);

    analysis_scheduler_tick("/tmp/test_project", "-I/tmp");
    assert(strcmp(g_last_started_live_contents, second) == 0);
    assert(g_last_started_live_revision == 11u);
}

static void test_live_buffer_preempts_pending_symbols_but_not_save(void) {
    reset_stub_state();
    analysis_scheduler_init();

    analysis_scheduler_request_key(ANALYSIS_JOB_KEY_SYMBOLS,
                                   ANALYSIS_REASON_EDITOR_EDIT_TRANSACTION,
                                   false);
    const char* contents = "int live;\n";
    analysis_scheduler_request_live_buffer("/tmp/test_project/src/live.c",
                                           contents,
                                           strlen(contents),
                                           12u,
                                           ANALYSIS_REASON_EDITOR_LIVE_BUFFER,
                                           false);

    analysis_scheduler_tick("/tmp/test_project", "-I/tmp");
    assert(g_worker_running);
    AnalysisSchedulerSnapshot snapshot;
    memset(&snapshot, 0, sizeof(snapshot));
    analysis_scheduler_snapshot(&snapshot);
    assert(snapshot.active_job_key == ANALYSIS_JOB_KEY_LIVE_DIAGNOSTICS);
    assert(snapshot.pending_key_mask & (1u << ANALYSIS_JOB_KEY_SYMBOLS));

    reset_stub_state();
    analysis_scheduler_init();
    analysis_scheduler_request_live_buffer("/tmp/test_project/src/live.c",
                                           contents,
                                           strlen(contents),
                                           12u,
                                           ANALYSIS_REASON_EDITOR_LIVE_BUFFER,
                                           false);
    analysis_scheduler_request_file("/tmp/test_project/src/saved.c",
                                    ANALYSIS_REASON_EDITOR_SAVE,
                                    false);
    analysis_scheduler_tick("/tmp/test_project", "-I/tmp");
    memset(&snapshot, 0, sizeof(snapshot));
    analysis_scheduler_snapshot(&snapshot);
    assert(snapshot.active_job_key == ANALYSIS_JOB_KEY_DIAGNOSTICS);
    assert(snapshot.active_reason_mask & ANALYSIS_REASON_EDITOR_SAVE);
    assert(snapshot.pending_key_mask & (1u << ANALYSIS_JOB_KEY_LIVE_DIAGNOSTICS));
}

int main(void) {
    test_same_key_latest_wins_coalescing();
    test_distinct_keys_queue_and_tick_order();
    test_index_lane_reason_normalizes_to_index_key();
    test_editor_save_reason_is_reported();
    test_file_hint_reaches_worker_start();
    test_editor_save_diagnostics_preempts_pending_symbols();
    test_editor_save_cancels_active_startup_workspace_run();
    test_live_buffer_snapshot_reaches_worker_start();
    test_live_buffer_latest_snapshot_wins();
    test_live_buffer_preempts_pending_symbols_but_not_save();
    puts("analysis_scheduler_coalescing_test: success");
    return 0;
}
