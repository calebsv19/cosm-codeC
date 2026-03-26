#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>


#include "event_loop.h"
#include "system_control.h"
#include "core_state.h"

#include "ide/Panes/Editor/editor_view.h"
#include "ide/Panes/Editor/editor_state.h"

#include "core/InputManager/input_manager.h"
#include "core/CommandBus/command_bus.h"
#include "core/Watcher/file_watcher.h"
#include "core/Analysis/project_scan.h"
#include "core/Analysis/analysis_status.h"
#include "core/Analysis/analysis_store.h"
#include "core/Analysis/analysis_symbols_store.h"
#include "core/InputManager/UserInput/rename_flow.h"
#include "ide/Panes/Terminal/terminal.h"
#include "ide/Panes/Terminal/input_terminal.h"
#include "ide/Panes/Popup/popup_pane.h"
#include "ide/Panes/ToolPanels/Project/tool_project.h"
#include "ide/Panes/ToolPanels/Assets/tool_assets.h"
#include "ide/Panes/ToolPanels/Git/render_tool_git.h"
#include "ide/Panes/ToolPanels/Git/tool_git.h"
#include "ide/Panes/ToolPanels/Libraries/tool_libraries.h"
#include "ide/Panes/ControlPanel/control_panel.h"

#include "ide/UI/layout.h"
#include "ide/UI/layout_config.h"
#include "ide/UI/ui_state.h"
#include "ide/UI/shared_theme_font_adapter.h"
#include "ide/UI/resize.h"
#include "engine/Render/render_pipeline.h"
#include "core/Analysis/library_index.h"
#include "core/Analysis/analysis_cache.h"
#include "core/Analysis/analysis_job.h"
#include "core/Analysis/analysis_scheduler.h"
#include "core/LoopWake/mainthread_wake.h"
#include "core/LoopTimer/mainthread_timer_scheduler.h"
#include "core/LoopResults/completed_results_queue.h"
#include "core/LoopEvents/event_queue.h"
#include "core/LoopEvents/event_invalidation_policy.h"
#include "core/LoopJobs/mainthread_jobs.h"
#include "core/LoopKernel/mainthread_context.h"
#include "core/LoopKernel/mainthread_kernel.h"
#include "core/LoopDiagnostics/loop_diag_config.h"
#include "core/LoopTime/loop_time.h"
#include "app/GlobalInfo/workspace_prefs.h"
#include "core/Ipc/ide_ipc_server.h"


// TimerHud extension
#include "timer_hud/time_scope.h"


//      ================================================
//                      Loop Logic


static EditorView* saveEditorViewState() {
    return (getCoreState()->editorPane) ? getCoreState()->editorPane->editorView : NULL;
}

typedef struct LayoutSyncState {
    int winW;
    int winH;
    int toolWidth;
    int controlWidth;
    int terminalHeight;
    bool toolPanelVisible;
    bool controlPanelVisible;
} LayoutSyncState;

static bool s_layout_sync_state_valid = false;
static LayoutSyncState s_layout_sync_state = {0};
static bool s_heartbeat_interval_initialized = false;
static float s_heartbeat_interval_seconds = 0.50f;
static bool s_force_full_redraw_initialized = false;
static bool s_force_full_redraw = false;
static bool s_loop_timers_registered = false;
static int s_file_watcher_timer_id = -1;
static int s_git_watcher_timer_id = -1;
static uint64_t s_latest_applied_analysis_run_id = 0;
static uint64_t s_analysis_progress_stamp = 0;
static uint64_t s_analysis_status_stamp = 0;
static bool s_loop_diag_initialized = false;
static bool s_loop_diag_enabled = false;
static bool s_loop_diag_json_output = false;
static int s_loop_max_wait_ms_override = -1;
static bool s_event_budget_initialized = false;
static int s_event_budget_per_frame = 128;

typedef struct LoopRuntimeDiag {
    Uint32 periodStartMs;
    Uint64 frames;
    Uint64 waitCalls;
    Uint64 blockedMs;
    Uint64 activeMs;
    Uint64 wakeDelta;
    Uint64 timerFiredDelta;
    uint32_t queueDepthPeak;
    uint32_t queueDepthLast;
    uint64_t jobsScheduledDelta;
    uint64_t jobsCoalescedDelta;
    uint64_t resultsAppliedDelta;
    uint64_t resultsStaleDroppedDelta;
    uint64_t editTxnStartsDelta;
    uint64_t editTxnCommitsDelta;
    uint64_t editTxnDebounceCommitsDelta;
    uint64_t editTxnBoundaryCommitsDelta;
    uint32_t eventQueueDepthPeak;
    uint32_t eventQueueDepthLast;
    uint64_t eventsEnqueuedDelta;
    uint64_t eventsProcessedDelta;
    uint64_t eventsDeferredDelta;
    uint64_t eventsDroppedOverflowDelta;
    uint64_t eventsEmitSymbolsDelta;
    uint64_t eventsEmitDiagnosticsDelta;
    uint64_t eventsEmitAnalysisProgressDelta;
    uint64_t eventsEmitAnalysisStatusDelta;
    uint64_t eventsEmitLibraryIndexDelta;
    uint64_t eventsEmitAnalysisFinishedDelta;
    uint64_t eventsDispatchSymbolsDelta;
    uint64_t eventsDispatchDiagnosticsDelta;
    uint64_t eventsDispatchAnalysisProgressDelta;
    uint64_t eventsDispatchAnalysisStatusDelta;
    uint64_t eventsDispatchLibraryIndexDelta;
    uint64_t eventsDispatchAnalysisFinishedDelta;
    uint64_t staleDropsSymbolsDelta;
    uint64_t staleDropsDiagnosticsDelta;
    uint64_t staleDropsAnalysisProgressDelta;
    uint64_t staleDropsAnalysisStatusDelta;
    uint64_t staleDropsAnalysisFinishedDelta;
    uint32_t lastWakeReceived;
    uint32_t lastTimerFired;
    uint64_t lastJobsScheduled;
    uint64_t lastJobsCoalesced;
    uint64_t lastResultsApplied;
    uint64_t lastResultsStaleDropped;
    uint64_t lastEditTxnStarts;
    uint64_t lastEditTxnCommits;
    uint64_t lastEditTxnDebounceCommits;
    uint64_t lastEditTxnBoundaryCommits;
    uint64_t lastEventsEnqueued;
    uint64_t lastEventsProcessed;
    uint64_t lastEventsDeferred;
    uint64_t lastEventsDroppedOverflow;
} LoopRuntimeDiag;

static LoopRuntimeDiag s_loop_diag = {0};

static void init_loop_diag_config(void) {
    if (s_loop_diag_initialized) return;
    LoopDiagConfig cfg = loop_diag_config_from_env();
    s_loop_diag_enabled = cfg.enabled;
    s_loop_diag_json_output = cfg.json_output;
    s_loop_max_wait_ms_override = cfg.max_wait_ms_override;
    s_loop_diag_initialized = true;
}

static int event_budget_per_frame(void) {
    if (s_event_budget_initialized) return s_event_budget_per_frame;
    const char* env = getenv("IDE_EVENT_BUDGET_PER_FRAME");
    if (env && env[0]) {
        char* end = NULL;
        long v = strtol(env, &end, 10);
        if (end != env) {
            if (v < 16) v = 16;
            if (v > 2048) v = 2048;
            s_event_budget_per_frame = (int)v;
        }
    }
    s_event_budget_initialized = true;
    return s_event_budget_per_frame;
}

static void loop_timer_file_watcher_cb(void* user_data) {
    (void)user_data;
    pollFileWatcher();
}

static void loop_timer_git_watcher_cb(void* user_data) {
    (void)user_data;
    pollGitStatusWatcher();
}

static void tick_command_bus_job(void* user_data) {
    (void)user_data;
    tickCommandBus();
}

static void ensure_loop_timers_registered(void) {
    if (s_loop_timers_registered) return;
    s_file_watcher_timer_id = mainthread_timer_schedule_repeating(fileWatcherPollIntervalMs(),
                                                                  loop_timer_file_watcher_cb,
                                                                  NULL,
                                                                  "file_watcher");
    s_git_watcher_timer_id = mainthread_timer_schedule_repeating(gitStatusWatchIntervalMs(),
                                                                 loop_timer_git_watcher_cb,
                                                                 NULL,
                                                                 "git_watcher");
    s_loop_timers_registered = true;
}

static void loop_diag_tick(uint64_t frameStartNs, uint64_t blockedNs, bool didWaitCall) {
    init_loop_diag_config();
    if (!s_loop_diag_enabled) return;

    uint64_t nowNs = loop_time_now_ns();
    Uint32 nowMs = (Uint32)(nowNs / 1000000ULL);
    if (s_loop_diag.periodStartMs == 0) {
        s_loop_diag.periodStartMs = nowMs;
        MainThreadWakeStats wake = {0};
        mainthread_wake_snapshot(&wake);
        s_loop_diag.lastWakeReceived = wake.received;
        MainThreadTimerSchedulerStats timerStats = {0};
        mainthread_timer_scheduler_snapshot(&timerStats);
        s_loop_diag.lastTimerFired = timerStats.fired_count;
        AnalysisSchedulerCounters schedulerCounters = {0};
        analysis_scheduler_counters_snapshot(&schedulerCounters);
        s_loop_diag.lastJobsScheduled = schedulerCounters.jobs_scheduled;
        s_loop_diag.lastJobsCoalesced = schedulerCounters.jobs_coalesced_replaced;
        CompletedResultsQueueStats resultStats = {0};
        completed_results_queue_snapshot(&resultStats);
        s_loop_diag.lastResultsApplied = resultStats.results_applied;
        s_loop_diag.lastResultsStaleDropped = resultStats.results_stale_dropped;
        EditorEditTransactionSnapshot editTxn = {0};
        editor_edit_transaction_snapshot(&editTxn);
        s_loop_diag.lastEditTxnStarts = editTxn.starts;
        s_loop_diag.lastEditTxnCommits = editTxn.commits;
        s_loop_diag.lastEditTxnDebounceCommits = editTxn.debounce_commits;
        s_loop_diag.lastEditTxnBoundaryCommits = editTxn.boundary_commits;
        LoopEventsStats eventStats = {0};
        loop_events_snapshot(&eventStats);
        s_loop_diag.lastEventsEnqueued = eventStats.events_enqueued;
        s_loop_diag.lastEventsProcessed = eventStats.events_processed;
        s_loop_diag.lastEventsDeferred = eventStats.events_deferred;
        s_loop_diag.lastEventsDroppedOverflow = eventStats.events_dropped_overflow;
    }

    uint64_t frameElapsedNs = loop_time_diff_ns(nowNs, frameStartNs);
    uint64_t activeNs = (frameElapsedNs > blockedNs) ? (frameElapsedNs - blockedNs) : 0;
    Uint32 blockedMs = (Uint32)(blockedNs / 1000000ULL);
    Uint32 activeMs = (Uint32)(activeNs / 1000000ULL);

    s_loop_diag.frames++;
    s_loop_diag.blockedMs += blockedMs;
    s_loop_diag.activeMs += activeMs;
    if (didWaitCall) s_loop_diag.waitCalls++;

    MainThreadWakeStats wake = {0};
    mainthread_wake_snapshot(&wake);
    if (wake.received >= s_loop_diag.lastWakeReceived) {
        s_loop_diag.wakeDelta += (wake.received - s_loop_diag.lastWakeReceived);
    }
    s_loop_diag.lastWakeReceived = wake.received;

    MainThreadTimerSchedulerStats timerStats = {0};
    mainthread_timer_scheduler_snapshot(&timerStats);
    if (timerStats.fired_count >= s_loop_diag.lastTimerFired) {
        s_loop_diag.timerFiredDelta += (timerStats.fired_count - s_loop_diag.lastTimerFired);
    }
    s_loop_diag.lastTimerFired = timerStats.fired_count;

    CompletedResultsQueueStats resultStats = {0};
    completed_results_queue_snapshot(&resultStats);
    s_loop_diag.queueDepthLast = resultStats.total_depth;
    if (resultStats.total_depth > s_loop_diag.queueDepthPeak) {
        s_loop_diag.queueDepthPeak = resultStats.total_depth;
    }
    if (resultStats.results_applied >= s_loop_diag.lastResultsApplied) {
        s_loop_diag.resultsAppliedDelta += (resultStats.results_applied - s_loop_diag.lastResultsApplied);
    }
    s_loop_diag.lastResultsApplied = resultStats.results_applied;
    if (resultStats.results_stale_dropped >= s_loop_diag.lastResultsStaleDropped) {
        s_loop_diag.resultsStaleDroppedDelta +=
            (resultStats.results_stale_dropped - s_loop_diag.lastResultsStaleDropped);
    }
    s_loop_diag.lastResultsStaleDropped = resultStats.results_stale_dropped;

    EditorEditTransactionSnapshot editTxn = {0};
    editor_edit_transaction_snapshot(&editTxn);
    if (editTxn.starts >= s_loop_diag.lastEditTxnStarts) {
        s_loop_diag.editTxnStartsDelta += (editTxn.starts - s_loop_diag.lastEditTxnStarts);
    }
    s_loop_diag.lastEditTxnStarts = editTxn.starts;
    if (editTxn.commits >= s_loop_diag.lastEditTxnCommits) {
        s_loop_diag.editTxnCommitsDelta += (editTxn.commits - s_loop_diag.lastEditTxnCommits);
    }
    s_loop_diag.lastEditTxnCommits = editTxn.commits;
    if (editTxn.debounce_commits >= s_loop_diag.lastEditTxnDebounceCommits) {
        s_loop_diag.editTxnDebounceCommitsDelta +=
            (editTxn.debounce_commits - s_loop_diag.lastEditTxnDebounceCommits);
    }
    s_loop_diag.lastEditTxnDebounceCommits = editTxn.debounce_commits;
    if (editTxn.boundary_commits >= s_loop_diag.lastEditTxnBoundaryCommits) {
        s_loop_diag.editTxnBoundaryCommitsDelta +=
            (editTxn.boundary_commits - s_loop_diag.lastEditTxnBoundaryCommits);
    }
    s_loop_diag.lastEditTxnBoundaryCommits = editTxn.boundary_commits;

    LoopEventsStats eventStats = {0};
    loop_events_snapshot(&eventStats);
    s_loop_diag.eventQueueDepthLast = eventStats.depth;
    if (eventStats.depth > s_loop_diag.eventQueueDepthPeak) {
        s_loop_diag.eventQueueDepthPeak = eventStats.depth;
    }
    if (eventStats.events_enqueued >= s_loop_diag.lastEventsEnqueued) {
        s_loop_diag.eventsEnqueuedDelta += (eventStats.events_enqueued - s_loop_diag.lastEventsEnqueued);
    }
    s_loop_diag.lastEventsEnqueued = eventStats.events_enqueued;
    if (eventStats.events_processed >= s_loop_diag.lastEventsProcessed) {
        s_loop_diag.eventsProcessedDelta += (eventStats.events_processed - s_loop_diag.lastEventsProcessed);
    }
    s_loop_diag.lastEventsProcessed = eventStats.events_processed;
    if (eventStats.events_deferred >= s_loop_diag.lastEventsDeferred) {
        s_loop_diag.eventsDeferredDelta += (eventStats.events_deferred - s_loop_diag.lastEventsDeferred);
    }
    s_loop_diag.lastEventsDeferred = eventStats.events_deferred;
    if (eventStats.events_dropped_overflow >= s_loop_diag.lastEventsDroppedOverflow) {
        s_loop_diag.eventsDroppedOverflowDelta +=
            (eventStats.events_dropped_overflow - s_loop_diag.lastEventsDroppedOverflow);
    }
    s_loop_diag.lastEventsDroppedOverflow = eventStats.events_dropped_overflow;

    AnalysisSchedulerCounters schedulerCounters = {0};
    analysis_scheduler_counters_snapshot(&schedulerCounters);
    if (schedulerCounters.jobs_scheduled >= s_loop_diag.lastJobsScheduled) {
        s_loop_diag.jobsScheduledDelta +=
            (schedulerCounters.jobs_scheduled - s_loop_diag.lastJobsScheduled);
    }
    s_loop_diag.lastJobsScheduled = schedulerCounters.jobs_scheduled;
    if (schedulerCounters.jobs_coalesced_replaced >= s_loop_diag.lastJobsCoalesced) {
        s_loop_diag.jobsCoalescedDelta +=
            (schedulerCounters.jobs_coalesced_replaced - s_loop_diag.lastJobsCoalesced);
    }
    s_loop_diag.lastJobsCoalesced = schedulerCounters.jobs_coalesced_replaced;

    Uint32 periodMs = nowMs - s_loop_diag.periodStartMs;
    if (periodMs < 1000) return;

    Uint64 totalMs = s_loop_diag.blockedMs + s_loop_diag.activeMs;
    double blockedPct = (totalMs > 0) ? (100.0 * (double)s_loop_diag.blockedMs / (double)totalMs) : 0.0;
    double activePct = (totalMs > 0) ? (100.0 * (double)s_loop_diag.activeMs / (double)totalMs) : 0.0;
    if (s_loop_diag_json_output) {
        printf("{\"tag\":\"LoopDiag\",\"schema\":1,\"period_ms\":%u,\"frames\":%llu,"
               "\"wait_calls\":%llu,\"blocked_ms\":%llu,\"blocked_pct\":%.1f,"
               "\"active_ms\":%llu,\"active_pct\":%.1f,\"wakes\":%llu,\"timers\":%llu,"
               "\"results_queue\":{\"last\":%u,\"peak\":%u},"
               "\"jobs\":{\"scheduled\":%llu,\"coalesced\":%llu},"
               "\"results\":{\"applied\":%llu,\"stale_dropped\":%llu},"
               "\"edit_txn\":{\"starts\":%llu,\"commits\":%llu,\"debounce_commits\":%llu,\"boundary_commits\":%llu},"
               "\"events\":{\"queue_last\":%u,\"queue_peak\":%u,\"enqueued\":%llu,\"processed\":%llu,\"deferred\":%llu,\"dropped\":%llu,"
               "\"emit\":{\"symbols\":%llu,\"diagnostics\":%llu,\"analysis_progress\":%llu,\"analysis_status\":%llu,\"library_index\":%llu,\"analysis_finished\":%llu},"
               "\"dispatch\":{\"symbols\":%llu,\"diagnostics\":%llu,\"analysis_progress\":%llu,\"analysis_status\":%llu,\"library_index\":%llu,\"analysis_finished\":%llu}},"
               "\"stale_by_kind\":{\"symbols\":%llu,\"diagnostics\":%llu,\"analysis_progress\":%llu,\"analysis_status\":%llu,\"analysis_finished\":%llu}}\n",
               (unsigned int)periodMs,
               (unsigned long long)s_loop_diag.frames,
               (unsigned long long)s_loop_diag.waitCalls,
               (unsigned long long)s_loop_diag.blockedMs,
               blockedPct,
               (unsigned long long)s_loop_diag.activeMs,
               activePct,
               (unsigned long long)s_loop_diag.wakeDelta,
               (unsigned long long)s_loop_diag.timerFiredDelta,
               s_loop_diag.queueDepthLast,
               s_loop_diag.queueDepthPeak,
               (unsigned long long)s_loop_diag.jobsScheduledDelta,
               (unsigned long long)s_loop_diag.jobsCoalescedDelta,
               (unsigned long long)s_loop_diag.resultsAppliedDelta,
               (unsigned long long)s_loop_diag.resultsStaleDroppedDelta,
               (unsigned long long)s_loop_diag.editTxnStartsDelta,
               (unsigned long long)s_loop_diag.editTxnCommitsDelta,
               (unsigned long long)s_loop_diag.editTxnDebounceCommitsDelta,
               (unsigned long long)s_loop_diag.editTxnBoundaryCommitsDelta,
               s_loop_diag.eventQueueDepthLast,
               s_loop_diag.eventQueueDepthPeak,
               (unsigned long long)s_loop_diag.eventsEnqueuedDelta,
               (unsigned long long)s_loop_diag.eventsProcessedDelta,
               (unsigned long long)s_loop_diag.eventsDeferredDelta,
               (unsigned long long)s_loop_diag.eventsDroppedOverflowDelta,
               (unsigned long long)s_loop_diag.eventsEmitSymbolsDelta,
               (unsigned long long)s_loop_diag.eventsEmitDiagnosticsDelta,
               (unsigned long long)s_loop_diag.eventsEmitAnalysisProgressDelta,
               (unsigned long long)s_loop_diag.eventsEmitAnalysisStatusDelta,
               (unsigned long long)s_loop_diag.eventsEmitLibraryIndexDelta,
               (unsigned long long)s_loop_diag.eventsEmitAnalysisFinishedDelta,
               (unsigned long long)s_loop_diag.eventsDispatchSymbolsDelta,
               (unsigned long long)s_loop_diag.eventsDispatchDiagnosticsDelta,
               (unsigned long long)s_loop_diag.eventsDispatchAnalysisProgressDelta,
               (unsigned long long)s_loop_diag.eventsDispatchAnalysisStatusDelta,
               (unsigned long long)s_loop_diag.eventsDispatchLibraryIndexDelta,
               (unsigned long long)s_loop_diag.eventsDispatchAnalysisFinishedDelta,
               (unsigned long long)s_loop_diag.staleDropsSymbolsDelta,
               (unsigned long long)s_loop_diag.staleDropsDiagnosticsDelta,
               (unsigned long long)s_loop_diag.staleDropsAnalysisProgressDelta,
               (unsigned long long)s_loop_diag.staleDropsAnalysisStatusDelta,
               (unsigned long long)s_loop_diag.staleDropsAnalysisFinishedDelta);
    } else {
        printf("[LoopDiag] period=%ums frames=%llu waits=%llu blocked=%llums(%.1f%%) active=%llums(%.1f%%) wakes=%llu timers=%llu q_last=%u q_peak=%u jobs=%llu coalesced=%llu applied=%llu stale_dropped=%llu edit_txn_starts=%llu commits=%llu debounce_commits=%llu boundary_commits=%llu ev_q_last=%u ev_q_peak=%u ev_enq=%llu ev_proc=%llu ev_deferred=%llu ev_dropped=%llu ev_emit[sym=%llu diag=%llu prog=%llu status=%llu idx=%llu fin=%llu] ev_dispatch[sym=%llu diag=%llu prog=%llu status=%llu idx=%llu fin=%llu] stale_by_kind[sym=%llu diag=%llu prog=%llu status=%llu fin=%llu]\n",
               (unsigned int)periodMs,
               (unsigned long long)s_loop_diag.frames,
               (unsigned long long)s_loop_diag.waitCalls,
               (unsigned long long)s_loop_diag.blockedMs,
               blockedPct,
               (unsigned long long)s_loop_diag.activeMs,
               activePct,
               (unsigned long long)s_loop_diag.wakeDelta,
               (unsigned long long)s_loop_diag.timerFiredDelta,
               s_loop_diag.queueDepthLast,
               s_loop_diag.queueDepthPeak,
               (unsigned long long)s_loop_diag.jobsScheduledDelta,
               (unsigned long long)s_loop_diag.jobsCoalescedDelta,
               (unsigned long long)s_loop_diag.resultsAppliedDelta,
               (unsigned long long)s_loop_diag.resultsStaleDroppedDelta,
               (unsigned long long)s_loop_diag.editTxnStartsDelta,
               (unsigned long long)s_loop_diag.editTxnCommitsDelta,
               (unsigned long long)s_loop_diag.editTxnDebounceCommitsDelta,
               (unsigned long long)s_loop_diag.editTxnBoundaryCommitsDelta,
               s_loop_diag.eventQueueDepthLast,
               s_loop_diag.eventQueueDepthPeak,
               (unsigned long long)s_loop_diag.eventsEnqueuedDelta,
               (unsigned long long)s_loop_diag.eventsProcessedDelta,
               (unsigned long long)s_loop_diag.eventsDeferredDelta,
               (unsigned long long)s_loop_diag.eventsDroppedOverflowDelta,
               (unsigned long long)s_loop_diag.eventsEmitSymbolsDelta,
               (unsigned long long)s_loop_diag.eventsEmitDiagnosticsDelta,
               (unsigned long long)s_loop_diag.eventsEmitAnalysisProgressDelta,
               (unsigned long long)s_loop_diag.eventsEmitAnalysisStatusDelta,
               (unsigned long long)s_loop_diag.eventsEmitLibraryIndexDelta,
               (unsigned long long)s_loop_diag.eventsEmitAnalysisFinishedDelta,
               (unsigned long long)s_loop_diag.eventsDispatchSymbolsDelta,
               (unsigned long long)s_loop_diag.eventsDispatchDiagnosticsDelta,
               (unsigned long long)s_loop_diag.eventsDispatchAnalysisProgressDelta,
               (unsigned long long)s_loop_diag.eventsDispatchAnalysisStatusDelta,
               (unsigned long long)s_loop_diag.eventsDispatchLibraryIndexDelta,
               (unsigned long long)s_loop_diag.eventsDispatchAnalysisFinishedDelta,
               (unsigned long long)s_loop_diag.staleDropsSymbolsDelta,
               (unsigned long long)s_loop_diag.staleDropsDiagnosticsDelta,
               (unsigned long long)s_loop_diag.staleDropsAnalysisProgressDelta,
               (unsigned long long)s_loop_diag.staleDropsAnalysisStatusDelta,
               (unsigned long long)s_loop_diag.staleDropsAnalysisFinishedDelta);
    }

    s_loop_diag.periodStartMs = nowMs;
    s_loop_diag.frames = 0;
    s_loop_diag.waitCalls = 0;
    s_loop_diag.blockedMs = 0;
    s_loop_diag.activeMs = 0;
    s_loop_diag.wakeDelta = 0;
    s_loop_diag.timerFiredDelta = 0;
    s_loop_diag.queueDepthPeak = 0;
    s_loop_diag.jobsScheduledDelta = 0;
    s_loop_diag.jobsCoalescedDelta = 0;
    s_loop_diag.resultsAppliedDelta = 0;
    s_loop_diag.resultsStaleDroppedDelta = 0;
    s_loop_diag.editTxnStartsDelta = 0;
    s_loop_diag.editTxnCommitsDelta = 0;
    s_loop_diag.editTxnDebounceCommitsDelta = 0;
    s_loop_diag.editTxnBoundaryCommitsDelta = 0;
    s_loop_diag.eventQueueDepthPeak = 0;
    s_loop_diag.eventsEnqueuedDelta = 0;
    s_loop_diag.eventsProcessedDelta = 0;
    s_loop_diag.eventsDeferredDelta = 0;
    s_loop_diag.eventsDroppedOverflowDelta = 0;
    s_loop_diag.eventsEmitSymbolsDelta = 0;
    s_loop_diag.eventsEmitDiagnosticsDelta = 0;
    s_loop_diag.eventsEmitAnalysisProgressDelta = 0;
    s_loop_diag.eventsEmitAnalysisStatusDelta = 0;
    s_loop_diag.eventsEmitLibraryIndexDelta = 0;
    s_loop_diag.eventsEmitAnalysisFinishedDelta = 0;
    s_loop_diag.eventsDispatchSymbolsDelta = 0;
    s_loop_diag.eventsDispatchDiagnosticsDelta = 0;
    s_loop_diag.eventsDispatchAnalysisProgressDelta = 0;
    s_loop_diag.eventsDispatchAnalysisStatusDelta = 0;
    s_loop_diag.eventsDispatchLibraryIndexDelta = 0;
    s_loop_diag.eventsDispatchAnalysisFinishedDelta = 0;
    s_loop_diag.staleDropsSymbolsDelta = 0;
    s_loop_diag.staleDropsDiagnosticsDelta = 0;
    s_loop_diag.staleDropsAnalysisProgressDelta = 0;
    s_loop_diag.staleDropsAnalysisStatusDelta = 0;
    s_loop_diag.staleDropsAnalysisFinishedDelta = 0;
}

static bool completed_result_is_stale(const CompletedResult* result) {
    if (!result || !result->has_document_revision) return false;
    if (!result->document_path[0]) return true;
    uint64_t current_revision = 0;
    if (!editor_find_open_file_revision_by_path(result->document_path, &current_revision)) {
        return true;
    }
    return current_revision != result->document_revision;
}

static void loop_diag_note_event_emitted(IDEEventType type) {
    switch (type) {
        case IDE_EVENT_SYMBOL_TREE_UPDATED:
            s_loop_diag.eventsEmitSymbolsDelta++;
            break;
        case IDE_EVENT_DIAGNOSTICS_UPDATED:
            s_loop_diag.eventsEmitDiagnosticsDelta++;
            break;
        case IDE_EVENT_ANALYSIS_PROGRESS_UPDATED:
            s_loop_diag.eventsEmitAnalysisProgressDelta++;
            break;
        case IDE_EVENT_ANALYSIS_STATUS_UPDATED:
            s_loop_diag.eventsEmitAnalysisStatusDelta++;
            break;
        case IDE_EVENT_LIBRARY_INDEX_UPDATED:
            s_loop_diag.eventsEmitLibraryIndexDelta++;
            break;
        case IDE_EVENT_ANALYSIS_RUN_FINISHED:
            s_loop_diag.eventsEmitAnalysisFinishedDelta++;
            break;
        case IDE_EVENT_NONE:
        case IDE_EVENT_DOCUMENT_EDITED:
        case IDE_EVENT_DOCUMENT_REVISION_CHANGED:
        default:
            break;
    }
}

static void loop_diag_note_event_dispatched(IDEEventType type) {
    switch (type) {
        case IDE_EVENT_SYMBOL_TREE_UPDATED:
            s_loop_diag.eventsDispatchSymbolsDelta++;
            break;
        case IDE_EVENT_DIAGNOSTICS_UPDATED:
            s_loop_diag.eventsDispatchDiagnosticsDelta++;
            break;
        case IDE_EVENT_ANALYSIS_PROGRESS_UPDATED:
            s_loop_diag.eventsDispatchAnalysisProgressDelta++;
            break;
        case IDE_EVENT_ANALYSIS_STATUS_UPDATED:
            s_loop_diag.eventsDispatchAnalysisStatusDelta++;
            break;
        case IDE_EVENT_LIBRARY_INDEX_UPDATED:
            s_loop_diag.eventsDispatchLibraryIndexDelta++;
            break;
        case IDE_EVENT_ANALYSIS_RUN_FINISHED:
            s_loop_diag.eventsDispatchAnalysisFinishedDelta++;
            break;
        case IDE_EVENT_NONE:
        case IDE_EVENT_DOCUMENT_EDITED:
        case IDE_EVENT_DOCUMENT_REVISION_CHANGED:
        default:
            break;
    }
}

static void note_stale_drop_kind(CompletedResultKind kind) {
    completed_results_queue_note_stale_dropped();
    switch (kind) {
        case COMPLETED_RESULT_SYMBOLS_UPDATED:
            s_loop_diag.staleDropsSymbolsDelta++;
            break;
        case COMPLETED_RESULT_DIAGNOSTICS_UPDATED:
            s_loop_diag.staleDropsDiagnosticsDelta++;
            break;
        case COMPLETED_RESULT_ANALYSIS_PROGRESS:
            s_loop_diag.staleDropsAnalysisProgressDelta++;
            break;
        case COMPLETED_RESULT_ANALYSIS_STATUS_UPDATE:
            s_loop_diag.staleDropsAnalysisStatusDelta++;
            break;
        case COMPLETED_RESULT_ANALYSIS_FINISHED:
            s_loop_diag.staleDropsAnalysisFinishedDelta++;
            break;
        case COMPLETED_RESULT_NONE:
        default:
            break;
    }
}

static void apply_completed_result(FrameContext* ctx, const CompletedResult* result) {
    if (!ctx || !result) return;
    (void)ctx;
    mainthread_context_assert_owner("event_loop.apply_completed_result");

    uint64_t result_run_id = 0;
    if (result->kind == COMPLETED_RESULT_ANALYSIS_FINISHED) {
        result_run_id = result->payload.analysis_finished.analysis_run_id;
    } else if (result->kind == COMPLETED_RESULT_SYMBOLS_UPDATED) {
        result_run_id = result->payload.symbols_updated.analysis_run_id;
    } else if (result->kind == COMPLETED_RESULT_DIAGNOSTICS_UPDATED) {
        result_run_id = result->payload.diagnostics_updated.analysis_run_id;
    } else if (result->kind == COMPLETED_RESULT_ANALYSIS_PROGRESS) {
        result_run_id = result->payload.analysis_progress.analysis_run_id;
    } else if (result->kind == COMPLETED_RESULT_ANALYSIS_STATUS_UPDATE) {
        result_run_id = result->payload.analysis_status_update.analysis_run_id;
    }
    if (result_run_id > 0 && result_run_id < s_latest_applied_analysis_run_id) {
        note_stale_drop_kind(result->kind);
        return;
    }

    if (result->kind == COMPLETED_RESULT_SYMBOLS_UPDATED) {
        const CompletedResultSymbolsUpdatedPayload* p = &result->payload.symbols_updated;
        if (p->project_root[0] && strcmp(p->project_root, projectPath) != 0) {
            note_stale_drop_kind(result->kind);
            return;
        }
        if (analysis_symbols_store_combined_stamp() != p->symbols_stamp) {
            note_stale_drop_kind(result->kind);
            return;
        }
    } else if (result->kind == COMPLETED_RESULT_DIAGNOSTICS_UPDATED) {
        const CompletedResultDiagnosticsUpdatedPayload* p = &result->payload.diagnostics_updated;
        if (p->project_root[0] && strcmp(p->project_root, projectPath) != 0) {
            note_stale_drop_kind(result->kind);
            return;
        }
        if (analysis_store_combined_stamp() != p->diagnostics_stamp) {
            note_stale_drop_kind(result->kind);
            return;
        }
    } else if (result->kind == COMPLETED_RESULT_ANALYSIS_PROGRESS) {
        const CompletedResultAnalysisProgressPayload* p = &result->payload.analysis_progress;
        if (p->project_root[0] && strcmp(p->project_root, projectPath) != 0) {
            note_stale_drop_kind(result->kind);
            return;
        }
    } else if (result->kind == COMPLETED_RESULT_ANALYSIS_STATUS_UPDATE) {
        const CompletedResultAnalysisStatusUpdatePayload* p = &result->payload.analysis_status_update;
        if (p->project_root[0] && strcmp(p->project_root, projectPath) != 0) {
            note_stale_drop_kind(result->kind);
            return;
        }
    } else if (result->kind == COMPLETED_RESULT_ANALYSIS_FINISHED) {
        const CompletedResultAnalysisFinishedPayload* p = &result->payload.analysis_finished;
        if (p->project_root[0] && strcmp(p->project_root, projectPath) != 0) {
            note_stale_drop_kind(result->kind);
            return;
        }
        if (library_index_combined_stamp() != p->library_index_stamp) {
            note_stale_drop_kind(result->kind);
            return;
        }
    }

    if (completed_result_is_stale(result)) {
        note_stale_drop_kind(result->kind);
        return;
    }

    if (result->kind == COMPLETED_RESULT_ANALYSIS_FINISHED) {
        const CompletedResultAnalysisFinishedPayload* p = &result->payload.analysis_finished;
        if (p->project_root[0] && strcmp(p->project_root, projectPath) != 0) {
            note_stale_drop_kind(result->kind);
            return;
        }
        completed_results_queue_note_applied();
        if (loop_events_emit_library_index_updated(p->project_root,
                                                   p->analysis_run_id,
                                                   p->library_index_stamp)) {
            loop_diag_note_event_emitted(IDE_EVENT_LIBRARY_INDEX_UPDATED);
        }
        if (loop_events_emit_analysis_run_finished(p->project_root,
                                                   p->analysis_run_id,
                                                   p->library_index_stamp)) {
            loop_diag_note_event_emitted(IDE_EVENT_ANALYSIS_RUN_FINISHED);
        }
        if (result_run_id > s_latest_applied_analysis_run_id) {
            s_latest_applied_analysis_run_id = result_run_id;
        }
    } else if (result->kind == COMPLETED_RESULT_SYMBOLS_UPDATED) {
        const CompletedResultSymbolsUpdatedPayload* p = &result->payload.symbols_updated;
        editor_sync_active_file_projection_mode();
        completed_results_queue_note_applied();
        if (loop_events_emit_symbol_tree_updated(p->project_root, p->analysis_run_id, p->symbols_stamp)) {
            loop_diag_note_event_emitted(IDE_EVENT_SYMBOL_TREE_UPDATED);
        }
        if (result_run_id > s_latest_applied_analysis_run_id) {
            s_latest_applied_analysis_run_id = result_run_id;
        }
    } else if (result->kind == COMPLETED_RESULT_DIAGNOSTICS_UPDATED) {
        const CompletedResultDiagnosticsUpdatedPayload* p = &result->payload.diagnostics_updated;
        analysis_store_flatten_to_engine();
        completed_results_queue_note_applied();
        if (loop_events_emit_diagnostics_updated(p->project_root, p->analysis_run_id, p->diagnostics_stamp)) {
            loop_diag_note_event_emitted(IDE_EVENT_DIAGNOSTICS_UPDATED);
        }
        if (result_run_id > s_latest_applied_analysis_run_id) {
            s_latest_applied_analysis_run_id = result_run_id;
        }
    } else if (result->kind == COMPLETED_RESULT_ANALYSIS_PROGRESS) {
        const CompletedResultAnalysisProgressPayload* p = &result->payload.analysis_progress;
        analysis_status_set_progress(p->completed_files, p->total_files);
        s_analysis_progress_stamp++;
        completed_results_queue_note_applied();
        if (loop_events_emit_analysis_progress_updated(p->project_root,
                                                       p->analysis_run_id,
                                                       s_analysis_progress_stamp)) {
            loop_diag_note_event_emitted(IDE_EVENT_ANALYSIS_PROGRESS_UPDATED);
        }
        if (result_run_id > s_latest_applied_analysis_run_id) {
            s_latest_applied_analysis_run_id = result_run_id;
        }
    } else if (result->kind == COMPLETED_RESULT_ANALYSIS_STATUS_UPDATE) {
        const CompletedResultAnalysisStatusUpdatePayload* p = &result->payload.analysis_status_update;
        if (p->set_has_cache) {
            analysis_status_set_has_cache(p->has_cache);
        }
        if (p->set_last_error) {
            analysis_status_set_last_error(p->last_error[0] ? p->last_error : NULL);
        }
        if (p->set_status) {
            analysis_status_set((AnalysisStatus)p->status_value);
        }
        s_analysis_status_stamp++;
        completed_results_queue_note_applied();
        if (loop_events_emit_analysis_status_updated(p->project_root,
                                                     p->analysis_run_id,
                                                     s_analysis_status_stamp)) {
            loop_diag_note_event_emitted(IDE_EVENT_ANALYSIS_STATUS_UPDATED);
        }
        if (result_run_id > s_latest_applied_analysis_run_id) {
            s_latest_applied_analysis_run_id = result_run_id;
        }
    }
}

static int drain_completed_results(FrameContext* ctx) {
    enum { kDrainBudget = 64 };
    int applied = 0;
    CompletedResult result;
    while (applied < kDrainBudget && completed_results_queue_pop_any(&result)) {
        apply_completed_result(ctx, &result);
        completed_results_queue_release(&result);
        applied++;
    }

    return applied;
}

static void invalidate_runtime_event_targets(const IDEEvent* event, FrameContext* ctx) {
    if (!event || !ctx) return;
    UIState* ui = getUIState();
    LoopEventInvalidationDispatchTargets targets = {
        .editor_pane = ui ? ui->editorPanel : NULL,
        .control_pane = ui ? ui->controlPanel : NULL,
        .tool_pane = ui ? ui->toolPanel : NULL,
        .all_panes = ctx->panes,
        .all_pane_count = ctx->paneCount ? *ctx->paneCount : 0
    };
    loop_events_dispatch_invalidation(event, &targets);
}

static void dispatch_runtime_event(const IDEEvent* event, void* user_data) {
    mainthread_context_assert_owner("event_loop.dispatch_runtime_event");
    if (event) {
        loop_diag_note_event_dispatched(event->type);
    }
    if (event && event->type == IDE_EVENT_SYMBOL_TREE_UPDATED) {
        control_panel_note_symbol_store_updated(event->payload.analysis.project_root,
                                                event->payload.analysis.data_stamp);
    } else if (event && event->type == IDE_EVENT_DIAGNOSTICS_UPDATED) {
        analysis_store_mark_published(event->payload.analysis.data_stamp);
    } else if (event && event->type == IDE_EVENT_LIBRARY_INDEX_UPDATED) {
        library_index_mark_published(event->payload.analysis.data_stamp);
        rebuildLibraryFlatRows();
    }
    FrameContext* ctx = (FrameContext*)user_data;
    invalidate_runtime_event_targets(event, ctx);
}

static int process_events_bounded(FrameContext* ctx) {
    int budget = event_budget_per_frame();
    if (budget < 0) budget = 0;
    return (int)loop_events_drain_bounded((size_t)budget, dispatch_runtime_event, ctx);
}

static float getHeartbeatIntervalSeconds(void) {
    if (!s_heartbeat_interval_initialized) {
        const char* env = getenv("IDE_RENDER_HEARTBEAT_MS");
        if (env && env[0]) {
            char* end = NULL;
            long ms = strtol(env, &end, 10);
            if (end != env && ms >= 50 && ms <= 2000) {
                s_heartbeat_interval_seconds = (float)ms / 1000.0f;
            }
        }
        s_heartbeat_interval_initialized = true;
    }
    return s_heartbeat_interval_seconds;
}

static bool forceFullRedrawEnabled(void) {
    if (!s_force_full_redraw_initialized) {
        const char* env = getenv("IDE_FORCE_FULL_REDRAW");
        s_force_full_redraw = (env && env[0] &&
                               (strcmp(env, "1") == 0 || strcasecmp(env, "true") == 0));
        s_force_full_redraw_initialized = true;
    }
    return s_force_full_redraw;
}

static bool workspace_has_diagnostics_cache(const char* project_root) {
    if (!project_root || !project_root[0]) return false;
    char path[1024];
    snprintf(path, sizeof(path), "%s/ide_files/analysis_diagnostics.json", project_root);
    struct stat st;
    return (stat(path, &st) == 0) && S_ISREG(st.st_mode);
}

static bool update_layout_sync_state_snapshot(void) {
    RenderContext* rctx = getRenderContext();
    if (!rctx || !rctx->window) return true;

    int winW = 0, winH = 0;
    SDL_GetWindowSize(rctx->window, &winW, &winH);

    LayoutDimensions* dims = getLayoutDimensions();
    UIState* ui = getUIState();
    LayoutSyncState next;
    memset(&next, 0, sizeof(next));
    next.winW = winW;
    next.winH = winH;
    next.toolWidth = dims ? dims->toolWidth : 0;
    next.controlWidth = dims ? dims->controlWidth : 0;
    next.terminalHeight = dims ? dims->terminalHeight : 0;
    next.toolPanelVisible = ui ? ui->toolPanelVisible : false;
    next.controlPanelVisible = ui ? ui->controlPanelVisible : false;

    if (!s_layout_sync_state_valid ||
        memcmp(&s_layout_sync_state, &next, sizeof(next)) != 0) {
        s_layout_sync_state = next;
        s_layout_sync_state_valid = true;
        return true;
    }
    return false;
}

static void layoutAndSyncPanes(UIPane** panes, int* paneCount) {
    layout_static_panes(panes, paneCount);
}

static bool shouldInvalidateForEvent(const SDL_Event* event) {
    if (!event) return false;

    switch (event->type) {
        case SDL_KEYDOWN:
        case SDL_KEYUP:
        case SDL_TEXTINPUT:
        case SDL_TEXTEDITING:
        case SDL_MOUSEMOTION:
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
        case SDL_MOUSEWHEEL:
        case SDL_DROPFILE:
        case SDL_DROPTEXT:
        case SDL_DROPBEGIN:
        case SDL_DROPCOMPLETE:
            return true;
        case SDL_WINDOWEVENT:
            switch (event->window.event) {
                case SDL_WINDOWEVENT_RESIZED:
                case SDL_WINDOWEVENT_SIZE_CHANGED:
                case SDL_WINDOWEVENT_EXPOSED:
                    return true;
                default:
                    return false;
            }
        default:
            return false;
    }
}

static void invalidateInputTargetPanes(FrameContext* ctx,
                                       const SDL_Event* event,
                                       UIPane* beforeMousePane,
                                       UIPane* beforeFocusedPane) {
    IDECoreState* core = getCoreState();
    if (!core || !event) return;

    UIPane* afterMousePane = core->activeMousePane;
    UIPane* afterFocusedPane = core->focusedPane;

    switch (event->type) {
        case SDL_KEYDOWN:
        case SDL_KEYUP:
        case SDL_TEXTINPUT:
        case SDL_TEXTEDITING:
            if (afterFocusedPane) {
                invalidatePane(afterFocusedPane,
                               RENDER_INVALIDATION_INPUT | RENDER_INVALIDATION_CONTENT);
            } else if (core->editorPane) {
                invalidatePane(core->editorPane,
                               RENDER_INVALIDATION_INPUT | RENDER_INVALIDATION_CONTENT);
            }
            break;
        case SDL_MOUSEMOTION:
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
        case SDL_MOUSEWHEEL:
            if (beforeMousePane) {
                invalidatePane(beforeMousePane, RENDER_INVALIDATION_INPUT);
            }
            if (afterMousePane && afterMousePane != beforeMousePane) {
                invalidatePane(afterMousePane, RENDER_INVALIDATION_INPUT);
            }
            if (!afterMousePane && core->editorPane) {
                invalidatePane(core->editorPane, RENDER_INVALIDATION_INPUT);
            }
            break;
        case SDL_WINDOWEVENT:
            if (event->window.event == SDL_WINDOWEVENT_RESIZED ||
                event->window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
                event->window.event == SDL_WINDOWEVENT_EXPOSED) {
                invalidateAll(ctx->panes, *ctx->paneCount,
                              RENDER_INVALIDATION_LAYOUT | RENDER_INVALIDATION_RESIZE);
                requestFullRedraw(RENDER_INVALIDATION_LAYOUT | RENDER_INVALIDATION_RESIZE);
            }
            break;
        default:
            if (beforeFocusedPane) {
                invalidatePane(beforeFocusedPane, RENDER_INVALIDATION_INPUT);
            }
            if (afterFocusedPane && afterFocusedPane != beforeFocusedPane) {
                invalidatePane(afterFocusedPane, RENDER_INVALIDATION_INPUT);
            }
            break;
    }
}

static bool process_single_input_event(FrameContext* ctx,
                                       const SDL_Event* src,
                                       bool debugKeyLog) {
    if (!ctx || !src) return false;
    SDL_Event e = *src;
    if (mainthread_wake_is_event(&e)) {
        mainthread_wake_note_received();
        return false;
    }

    IDECoreState* core = getCoreState();
    UIPane* beforeMousePane = core ? core->activeMousePane : NULL;
    UIPane* beforeFocusedPane = core ? core->focusedPane : NULL;

    bool visualEvent = shouldInvalidateForEvent(&e);
    if (debugKeyLog && e.type == SDL_KEYDOWN) {
        SDL_Keycode key = e.key.keysym.sym;
        printf("KEYDOWN: %s (%d)\n", SDL_GetKeyName(key), key);
    }

    *ctx->event = e;
    handleInput(ctx->event, ctx->panes, *ctx->paneCount,
                ctx->resizeZones, *ctx->resizeZoneCount,
                ctx->paneCount, ctx->running);

    if (visualEvent) {
        invalidateInputTargetPanes(ctx, &e, beforeMousePane, beforeFocusedPane);
    }
    return visualEvent;
}

static bool processInputEvents(FrameContext* ctx, const SDL_Event* firstEvent) {
    bool sawVisualEvent = false;
    const char* debugKeysEnv = getenv("IDE_DEBUG_KEY_LOG");
    const bool debugKeyLog = (debugKeysEnv && debugKeysEnv[0] &&
                              (strcmp(debugKeysEnv, "1") == 0 ||
                               strcasecmp(debugKeysEnv, "true") == 0));

    bool havePendingMotion = false;
    SDL_Event pendingMotion;
    memset(&pendingMotion, 0, sizeof(pendingMotion));

    if (firstEvent) {
        if (firstEvent->type == SDL_MOUSEMOTION) {
            pendingMotion = *firstEvent;
            havePendingMotion = true;
        } else {
            sawVisualEvent |= process_single_input_event(ctx, firstEvent, debugKeyLog);
        }
    }

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_MOUSEMOTION) {
            pendingMotion = e;
            havePendingMotion = true;
            continue;
        }
        if (havePendingMotion) {
            sawVisualEvent |= process_single_input_event(ctx, &pendingMotion, debugKeyLog);
            havePendingMotion = false;
        }
        sawVisualEvent |= process_single_input_event(ctx, &e, debugKeyLog);
    }

    if (havePendingMotion) {
        sawVisualEvent |= process_single_input_event(ctx, &pendingMotion, debugKeyLog);
    }
    return sawVisualEvent;
}


static bool tickBackgroundSystems() {
    bool terminalChanged = false;
    ide_ipc_pump();
    mainthread_jobs_enqueue(tick_command_bus_job, NULL);
    mainthread_kernel_tick(loop_time_now_ns());
    terminalChanged = terminal_tick_backend();
    // tickDiagnosticsEngine(dt), tickUndoSystem(dt), etc.
    analysis_job_poll();
    return terminalChanged;
}

static bool main_loop_has_immediate_work(void) {
    if (pendingProjectRefresh) return true;
    if (hasFrameInvalidation()) return true;

    CompletedResultsQueueStats resultStats = {0};
    completed_results_queue_snapshot(&resultStats);
    if (resultStats.total_depth > 0) return true;
    if (loop_events_size() > 0u) return true;
    return false;
}

static int compute_wait_timeout_ms(FrameContext* ctx) {
    if (!ctx) return 0;

    IDECoreState* core = getCoreState();
    const bool activeInteraction =
        (core && core->projectDrag.active) || gResizeDrag.active || isRenaming();
    const bool invalidated = hasFrameInvalidation();

    const Uint64 perfNow = SDL_GetPerformanceCounter();
    const float freq = (float)SDL_GetPerformanceFrequency();
    const float elapsedSinceRender = (perfNow - *ctx->lastRender) / freq;

    int timeoutMs = -1;
    if (activeInteraction || invalidated) {
        float frameRemain = ctx->targetFrameTime - elapsedSinceRender;
        int frameMs = frameRemain > 0.0f ? (int)(frameRemain * 1000.0f) : 0;
        if (frameMs < 0) frameMs = 0;
        if (frameMs > 16) frameMs = 16;
        timeoutMs = frameMs;
    }

    Uint32 nextDeadlineMs = 0;
    if (mainthread_timer_scheduler_next_deadline_ms(&nextDeadlineMs)) {
        Uint32 nowMs = loop_time_now_ms32();
        int timerMs = (int)(nextDeadlineMs - nowMs);
        if (timerMs < 0) timerMs = 0;
        if (timeoutMs < 0 || timerMs < timeoutMs) timeoutMs = timerMs;
    }

    float heartbeat = getHeartbeatIntervalSeconds();
    int heartbeatMs = (int)((heartbeat - elapsedSinceRender) * 1000.0f);
    if (heartbeatMs < 0) heartbeatMs = 0;
    if (timeoutMs < 0 || heartbeatMs < timeoutMs) timeoutMs = heartbeatMs;

    if (timeoutMs < 0) timeoutMs = 500;
    if (!activeInteraction && timeoutMs > 500) timeoutMs = 500;
    init_loop_diag_config();
    if (s_loop_max_wait_ms_override > 0 && timeoutMs > s_loop_max_wait_ms_override) {
        timeoutMs = s_loop_max_wait_ms_override;
    }
    return timeoutMs;
}

static uint64_t wait_for_next_wake(FrameContext* ctx, bool* outWaitCalled) {
    if (outWaitCalled) *outWaitCalled = false;
    if (!ctx || !ctx->running || !(*ctx->running)) return 0;
    if (main_loop_has_immediate_work()) return 0;

    int timeoutMs = compute_wait_timeout_ms(ctx);
    if (timeoutMs < 0) return 0;

    if (outWaitCalled) *outWaitCalled = true;
    uint64_t waitStartNs = loop_time_now_ns();
    SDL_Event waitedEvent;
    if (mainthread_wake_wait_for_event((uint32_t)timeoutMs, &waitedEvent)) {
        processInputEvents(ctx, &waitedEvent);
    }
    uint64_t waitEndNs = loop_time_now_ns();
    return loop_time_diff_ns(waitEndNs, waitStartNs);
}

//                      Loop Logic
//      ================================================
//                      Render Logic


static bool checkRenderFrame(FrameContext* ctx, Uint64 now) {
    float timeSinceLastRender = (now - *ctx->lastRender) / (float)SDL_GetPerformanceFrequency();
    const bool invalidated = hasFrameInvalidation();
    const float heartbeat = getHeartbeatIntervalSeconds();
    const bool invalidationDue = invalidated && (timeSinceLastRender >= ctx->targetFrameTime);
    const bool heartbeatDue = (!invalidated) && (timeSinceLastRender >= heartbeat);

    if (invalidationDue || heartbeatDue) {
        const bool timerHudActive = isTimerHudEnabled();
        RenderContext* rctx = getRenderContext();

        // Clear background color before drawing
        if (timerHudActive) ts_start_timer("ClearFrame");
        {
            SDL_Color bg = ide_shared_theme_background_color();
            SDL_SetRenderDrawColor(rctx->renderer, bg.r, bg.g, bg.b, bg.a);
        }
#if !USE_VULKAN
        SDL_RenderClear(rctx->renderer);
#endif
        if (timerHudActive) ts_stop_timer("ClearFrame");

        // Render all UI
        if (timerHudActive) ts_start_timer("RenderPipeline");
        RenderPipeline_renderAll(ctx->panes,
                                 *ctx->paneCount,
                                 ctx->lastW, ctx->lastH,
                                 ctx->resizeZones, ctx->resizeZoneCount, getCoreState());
        if (timerHudActive) ts_stop_timer("RenderPipeline");

        *ctx->lastRender = now;
        return true;
    }
    return false;
}


//                      Render Logic
//      ================================================
//                          Main


void runFrameLoop(FrameContext* ctx, Uint64 now, float dt) {
    mainthread_context_assert_owner("event_loop.runFrameLoop");
    IDECoreState* core = getCoreState();
    const bool timerHudActive = core->timerHudEnabled;
    uint64_t frameStartNs = loop_time_now_ns();
    uint64_t blockedNs = 0;
    bool didWaitCall = false;

    if (timerHudActive) ts_start_timer("SystemLoop");

    ensure_loop_timers_registered();

    EditorView* savedView = saveEditorViewState();

    if (timerHudActive) ts_start_timer("LayoutSync");
    if (update_layout_sync_state_snapshot()) {
        layoutAndSyncPanes(ctx->panes, ctx->paneCount);
        bindEditorViewToEditorPane(savedView, ctx->panes, *ctx->paneCount);
        invalidateAll(ctx->panes, *ctx->paneCount,
                      RENDER_INVALIDATION_LAYOUT | RENDER_INVALIDATION_RESIZE);
        requestFullRedraw(RENDER_INVALIDATION_LAYOUT | RENDER_INVALIDATION_RESIZE);
    }
    if (timerHudActive) ts_stop_timer("LayoutSync");

    if (timerHudActive) ts_start_timer("Input");
    processInputEvents(ctx, NULL);
    UIState* ui = getUIState();
    if (ui && ui->terminalVisible && ui->terminalPanel) {
        if (terminal_tick_drag_autoscroll(ui->terminalPanel, dt)) {
            invalidatePane(ui->terminalPanel,
                           RENDER_INVALIDATION_INPUT | RENDER_INVALIDATION_CONTENT);
            requestFullRedraw(RENDER_INVALIDATION_INPUT | RENDER_INVALIDATION_CONTENT);
        }
    }
    if (timerHudActive) ts_stop_timer("Input");
    editor_edit_transaction_update_active_context();

    if (forceFullRedrawEnabled()) {
        invalidateAll(ctx->panes, *ctx->paneCount,
                      RENDER_INVALIDATION_LAYOUT | RENDER_INVALIDATION_CONTENT);
        requestFullRedraw(RENDER_INVALIDATION_LAYOUT | RENDER_INVALIDATION_CONTENT);
    }

    if (timerHudActive) ts_start_timer("BackgroundTick");
    const bool terminalChanged = tickBackgroundSystems();
    drain_completed_results(ctx);
    process_events_bounded(ctx);
    if (terminalChanged) {
        UIState* ui = getUIState();
        if (ui && ui->terminalPanel && ui->terminalVisible) {
            invalidatePane(ui->terminalPanel,
                           RENDER_INVALIDATION_CONTENT | RENDER_INVALIDATION_BACKGROUND);
        }
    }
    const WorkspaceBuildConfig* cfg = getWorkspaceBuildConfig();
    const char* buildArgs = (cfg && cfg->build_args[0]) ? cfg->build_args : NULL;
    analysis_scheduler_tick(projectPath, buildArgs);
    if (timerHudActive) ts_stop_timer("BackgroundTick");

    if (tickRenameAnimation(loop_time_now_ms32())) {
        requestFullRedraw(RENDER_INVALIDATION_OVERLAY);
    }

    if (core->projectDrag.active) {
        requestFullRedraw(RENDER_INVALIDATION_OVERLAY);
    }

    if (timerHudActive) ts_start_timer("RenderGate");
    bool didRender = checkRenderFrame(ctx, now);
    if (timerHudActive) ts_stop_timer("RenderGate");

    if (pendingProjectRefresh) {
        if (timerHudActive) ts_start_timer("ProjectRefresh");
        unsigned int reason_mask = pendingProjectRefreshReasonMask;
        if (reason_mask == 0) {
            reason_mask = ANALYSIS_REASON_WORKSPACE_RELOAD;
        }
        bool force_full = false;
        bool slow_mode = false;
        if (reason_mask & ANALYSIS_REASON_WORKSPACE_RELOAD) {
            bool has_diag_cache = workspace_has_diagnostics_cache(projectPath);
            force_full = !has_diag_cache; // No cache => quick full rebuild.
            slow_mode = has_diag_cache;   // Cache exists => lazy background mode.
        }
        refreshProjectDirectory();
        analysis_status_set(ANALYSIS_STATUS_STALE_LOADING);
        s_analysis_status_stamp++;
        if (loop_events_emit_analysis_status_updated(projectPath, 0u, s_analysis_status_stamp)) {
            loop_diag_note_event_emitted(IDE_EVENT_ANALYSIS_STATUS_UPDATED);
        }
        analysis_job_set_slow_mode_next_run(slow_mode);
        analysis_scheduler_request((AnalysisRefreshReason)reason_mask, force_full);
        if (loop_events_emit_library_index_updated(projectPath, 0u, library_index_combined_stamp())) {
            loop_diag_note_event_emitted(IDE_EVENT_LIBRARY_INDEX_UPDATED);
        }
        initAssetManagerPanel();
        resetGitTree();
        pendingProjectRefresh = false;
        pendingProjectRefreshReasonMask = 0;
        invalidateAll(ctx->panes, *ctx->paneCount,
                      RENDER_INVALIDATION_CONTENT | RENDER_INVALIDATION_BACKGROUND);
        requestFullRedraw(RENDER_INVALIDATION_CONTENT | RENDER_INVALIDATION_LAYOUT);
        if (timerHudActive) ts_stop_timer("ProjectRefresh");
    }

    if (didRender) {
        uint64_t renderedFrameId = 0;
        if (consumeFrameInvalidation(NULL, NULL, &renderedFrameId)) {
            for (int i = 0; i < *ctx->paneCount; ++i) {
                UIPane* pane = ctx->panes[i];
                if (!pane) continue;
                pane->lastRenderFrameId = renderedFrameId;
                paneClearDirty(pane);
            }
        }
    }

    if (!didRender) {
        blockedNs = wait_for_next_wake(ctx, &didWaitCall);
    }
    loop_diag_tick(frameStartNs, blockedNs, didWaitCall);

    if (timerHudActive) ts_stop_timer("SystemLoop");
}
