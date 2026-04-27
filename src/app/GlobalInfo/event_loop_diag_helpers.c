#include "app/GlobalInfo/event_loop_diag_helpers.h"

#include <SDL2/SDL.h>
#include <stdio.h>

#include "core/Analysis/analysis_scheduler.h"
#include "ide/Panes/Editor/editor_view.h"
#include "core/LoopDiagnostics/loop_diag_config.h"
#include "core/LoopResults/completed_results_queue.h"
#include "core/LoopTime/loop_time.h"
#include "core/LoopTimer/mainthread_timer_scheduler.h"
#include "core/LoopWake/mainthread_wake.h"

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

static bool s_loop_diag_initialized = false;
static bool s_loop_diag_enabled = false;
static bool s_loop_diag_json_output = false;
static int s_loop_max_wait_ms_override = -1;
static LoopRuntimeDiag s_loop_diag = {0};

static void init_loop_diag_config(void) {
    if (s_loop_diag_initialized) return;
    LoopDiagConfig cfg = loop_diag_config_from_env();
    s_loop_diag_enabled = cfg.enabled;
    s_loop_diag_json_output = cfg.json_output;
    s_loop_max_wait_ms_override = cfg.max_wait_ms_override;
    s_loop_diag_initialized = true;
}

void event_loop_diag_tick(uint64_t frame_start_ns, uint64_t blocked_ns, bool did_wait_call) {
    init_loop_diag_config();
    if (!s_loop_diag_enabled) return;

    uint64_t now_ns = loop_time_now_ns();
    Uint32 now_ms = (Uint32)(now_ns / 1000000ULL);
    if (s_loop_diag.periodStartMs == 0) {
        s_loop_diag.periodStartMs = now_ms;
        MainThreadWakeStats wake = {0};
        mainthread_wake_snapshot(&wake);
        s_loop_diag.lastWakeReceived = wake.received;
        MainThreadTimerSchedulerStats timer_stats = {0};
        mainthread_timer_scheduler_snapshot(&timer_stats);
        s_loop_diag.lastTimerFired = timer_stats.fired_count;
        AnalysisSchedulerCounters scheduler_counters = {0};
        analysis_scheduler_counters_snapshot(&scheduler_counters);
        s_loop_diag.lastJobsScheduled = scheduler_counters.jobs_scheduled;
        s_loop_diag.lastJobsCoalesced = scheduler_counters.jobs_coalesced_replaced;
        CompletedResultsQueueStats result_stats = {0};
        completed_results_queue_snapshot(&result_stats);
        s_loop_diag.lastResultsApplied = result_stats.results_applied;
        s_loop_diag.lastResultsStaleDropped = result_stats.results_stale_dropped;
        EditorEditTransactionSnapshot edit_txn = {0};
        editor_edit_transaction_snapshot(&edit_txn);
        s_loop_diag.lastEditTxnStarts = edit_txn.starts;
        s_loop_diag.lastEditTxnCommits = edit_txn.commits;
        s_loop_diag.lastEditTxnDebounceCommits = edit_txn.debounce_commits;
        s_loop_diag.lastEditTxnBoundaryCommits = edit_txn.boundary_commits;
        LoopEventsStats event_stats = {0};
        loop_events_snapshot(&event_stats);
        s_loop_diag.lastEventsEnqueued = event_stats.events_enqueued;
        s_loop_diag.lastEventsProcessed = event_stats.events_processed;
        s_loop_diag.lastEventsDeferred = event_stats.events_deferred;
        s_loop_diag.lastEventsDroppedOverflow = event_stats.events_dropped_overflow;
    }

    uint64_t frame_elapsed_ns = loop_time_diff_ns(now_ns, frame_start_ns);
    uint64_t active_ns = (frame_elapsed_ns > blocked_ns) ? (frame_elapsed_ns - blocked_ns) : 0;
    Uint32 blocked_ms = (Uint32)(blocked_ns / 1000000ULL);
    Uint32 active_ms = (Uint32)(active_ns / 1000000ULL);

    s_loop_diag.frames++;
    s_loop_diag.blockedMs += blocked_ms;
    s_loop_diag.activeMs += active_ms;
    if (did_wait_call) s_loop_diag.waitCalls++;

    MainThreadWakeStats wake = {0};
    mainthread_wake_snapshot(&wake);
    if (wake.received >= s_loop_diag.lastWakeReceived) {
        s_loop_diag.wakeDelta += (wake.received - s_loop_diag.lastWakeReceived);
    }
    s_loop_diag.lastWakeReceived = wake.received;

    MainThreadTimerSchedulerStats timer_stats = {0};
    mainthread_timer_scheduler_snapshot(&timer_stats);
    if (timer_stats.fired_count >= s_loop_diag.lastTimerFired) {
        s_loop_diag.timerFiredDelta += (timer_stats.fired_count - s_loop_diag.lastTimerFired);
    }
    s_loop_diag.lastTimerFired = timer_stats.fired_count;

    CompletedResultsQueueStats result_stats = {0};
    completed_results_queue_snapshot(&result_stats);
    s_loop_diag.queueDepthLast = result_stats.total_depth;
    if (result_stats.total_depth > s_loop_diag.queueDepthPeak) {
        s_loop_diag.queueDepthPeak = result_stats.total_depth;
    }
    if (result_stats.results_applied >= s_loop_diag.lastResultsApplied) {
        s_loop_diag.resultsAppliedDelta += (result_stats.results_applied - s_loop_diag.lastResultsApplied);
    }
    s_loop_diag.lastResultsApplied = result_stats.results_applied;
    if (result_stats.results_stale_dropped >= s_loop_diag.lastResultsStaleDropped) {
        s_loop_diag.resultsStaleDroppedDelta +=
            (result_stats.results_stale_dropped - s_loop_diag.lastResultsStaleDropped);
    }
    s_loop_diag.lastResultsStaleDropped = result_stats.results_stale_dropped;

    EditorEditTransactionSnapshot edit_txn = {0};
    editor_edit_transaction_snapshot(&edit_txn);
    if (edit_txn.starts >= s_loop_diag.lastEditTxnStarts) {
        s_loop_diag.editTxnStartsDelta += (edit_txn.starts - s_loop_diag.lastEditTxnStarts);
    }
    s_loop_diag.lastEditTxnStarts = edit_txn.starts;
    if (edit_txn.commits >= s_loop_diag.lastEditTxnCommits) {
        s_loop_diag.editTxnCommitsDelta += (edit_txn.commits - s_loop_diag.lastEditTxnCommits);
    }
    s_loop_diag.lastEditTxnCommits = edit_txn.commits;
    if (edit_txn.debounce_commits >= s_loop_diag.lastEditTxnDebounceCommits) {
        s_loop_diag.editTxnDebounceCommitsDelta +=
            (edit_txn.debounce_commits - s_loop_diag.lastEditTxnDebounceCommits);
    }
    s_loop_diag.lastEditTxnDebounceCommits = edit_txn.debounce_commits;
    if (edit_txn.boundary_commits >= s_loop_diag.lastEditTxnBoundaryCommits) {
        s_loop_diag.editTxnBoundaryCommitsDelta +=
            (edit_txn.boundary_commits - s_loop_diag.lastEditTxnBoundaryCommits);
    }
    s_loop_diag.lastEditTxnBoundaryCommits = edit_txn.boundary_commits;

    LoopEventsStats event_stats = {0};
    loop_events_snapshot(&event_stats);
    s_loop_diag.eventQueueDepthLast = event_stats.depth;
    if (event_stats.depth > s_loop_diag.eventQueueDepthPeak) {
        s_loop_diag.eventQueueDepthPeak = event_stats.depth;
    }
    if (event_stats.events_enqueued >= s_loop_diag.lastEventsEnqueued) {
        s_loop_diag.eventsEnqueuedDelta += (event_stats.events_enqueued - s_loop_diag.lastEventsEnqueued);
    }
    s_loop_diag.lastEventsEnqueued = event_stats.events_enqueued;
    if (event_stats.events_processed >= s_loop_diag.lastEventsProcessed) {
        s_loop_diag.eventsProcessedDelta += (event_stats.events_processed - s_loop_diag.lastEventsProcessed);
    }
    s_loop_diag.lastEventsProcessed = event_stats.events_processed;
    if (event_stats.events_deferred >= s_loop_diag.lastEventsDeferred) {
        s_loop_diag.eventsDeferredDelta += (event_stats.events_deferred - s_loop_diag.lastEventsDeferred);
    }
    s_loop_diag.lastEventsDeferred = event_stats.events_deferred;
    if (event_stats.events_dropped_overflow >= s_loop_diag.lastEventsDroppedOverflow) {
        s_loop_diag.eventsDroppedOverflowDelta +=
            (event_stats.events_dropped_overflow - s_loop_diag.lastEventsDroppedOverflow);
    }
    s_loop_diag.lastEventsDroppedOverflow = event_stats.events_dropped_overflow;

    AnalysisSchedulerCounters scheduler_counters = {0};
    analysis_scheduler_counters_snapshot(&scheduler_counters);
    if (scheduler_counters.jobs_scheduled >= s_loop_diag.lastJobsScheduled) {
        s_loop_diag.jobsScheduledDelta +=
            (scheduler_counters.jobs_scheduled - s_loop_diag.lastJobsScheduled);
    }
    s_loop_diag.lastJobsScheduled = scheduler_counters.jobs_scheduled;
    if (scheduler_counters.jobs_coalesced_replaced >= s_loop_diag.lastJobsCoalesced) {
        s_loop_diag.jobsCoalescedDelta +=
            (scheduler_counters.jobs_coalesced_replaced - s_loop_diag.lastJobsCoalesced);
    }
    s_loop_diag.lastJobsCoalesced = scheduler_counters.jobs_coalesced_replaced;

    Uint32 period_ms = now_ms - s_loop_diag.periodStartMs;
    if (period_ms < 1000) return;

    Uint64 total_ms = s_loop_diag.blockedMs + s_loop_diag.activeMs;
    double blocked_pct = (total_ms > 0) ? (100.0 * (double)s_loop_diag.blockedMs / (double)total_ms) : 0.0;
    double active_pct = (total_ms > 0) ? (100.0 * (double)s_loop_diag.activeMs / (double)total_ms) : 0.0;
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
               (unsigned int)period_ms,
               (unsigned long long)s_loop_diag.frames,
               (unsigned long long)s_loop_diag.waitCalls,
               (unsigned long long)s_loop_diag.blockedMs,
               blocked_pct,
               (unsigned long long)s_loop_diag.activeMs,
               active_pct,
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
               (unsigned int)period_ms,
               (unsigned long long)s_loop_diag.frames,
               (unsigned long long)s_loop_diag.waitCalls,
               (unsigned long long)s_loop_diag.blockedMs,
               blocked_pct,
               (unsigned long long)s_loop_diag.activeMs,
               active_pct,
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

    s_loop_diag.periodStartMs = now_ms;
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

void event_loop_diag_note_event_emitted(IDEEventType type) {
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

void event_loop_diag_note_event_dispatched(IDEEventType type) {
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

void event_loop_diag_note_stale_drop_kind(CompletedResultKind kind) {
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

int event_loop_diag_clamp_wait_timeout_ms(int timeout_ms) {
    init_loop_diag_config();
    if (s_loop_max_wait_ms_override > 0 && timeout_ms > s_loop_max_wait_ms_override) {
        return s_loop_max_wait_ms_override;
    }
    return timeout_ms;
}
