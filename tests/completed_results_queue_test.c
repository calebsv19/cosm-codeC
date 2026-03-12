#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/LoopResults/completed_results_queue.h"

static int g_freed_owned_payloads = 0;

static void test_owned_payload_free(void* ptr) {
    if (ptr) {
        g_freed_owned_payloads++;
        free(ptr);
    }
}

static void push_basic_result(CompletedResultSubsystem subsystem,
                              CompletedResultKind kind,
                              int marker) {
    CompletedResult result;
    memset(&result, 0, sizeof(result));
    result.subsystem = subsystem;
    result.kind = kind;
    result.payload.analysis_progress.completed_files = marker;
    assert(completed_results_queue_push(&result));
}

static CompletedResultKind kind_for_subsystem(CompletedResultSubsystem subsystem) {
    switch (subsystem) {
        case COMPLETED_SUBSYSTEM_ANALYSIS:
            return COMPLETED_RESULT_ANALYSIS_PROGRESS;
        case COMPLETED_SUBSYSTEM_SYMBOLS:
            return COMPLETED_RESULT_SYMBOLS_UPDATED;
        case COMPLETED_SUBSYSTEM_DIAGNOSTICS:
            return COMPLETED_RESULT_DIAGNOSTICS_UPDATED;
        default:
            return COMPLETED_RESULT_NONE;
    }
}

static int fill_subsystem_until_full(CompletedResultSubsystem subsystem) {
    int pushed = 0;
    for (int i = 0; i < 5000; ++i) {
        CompletedResult result;
        memset(&result, 0, sizeof(result));
        result.subsystem = subsystem;
        result.kind = kind_for_subsystem(subsystem);
        if (!completed_results_queue_push(&result)) {
            break;
        }
        pushed++;
    }
    return pushed;
}

static void test_pop_any_global_ordering(void) {
    completed_results_queue_reset();

    push_basic_result(COMPLETED_SUBSYSTEM_SYMBOLS, COMPLETED_RESULT_SYMBOLS_UPDATED, 11);
    push_basic_result(COMPLETED_SUBSYSTEM_ANALYSIS, COMPLETED_RESULT_ANALYSIS_PROGRESS, 22);
    push_basic_result(COMPLETED_SUBSYSTEM_DIAGNOSTICS, COMPLETED_RESULT_DIAGNOSTICS_UPDATED, 33);

    CompletedResult out;
    assert(completed_results_queue_pop_any(&out));
    assert(out.subsystem == COMPLETED_SUBSYSTEM_SYMBOLS);
    assert(out.seq == 1);
    completed_results_queue_release(&out);

    assert(completed_results_queue_pop_any(&out));
    assert(out.subsystem == COMPLETED_SUBSYSTEM_ANALYSIS);
    assert(out.seq == 2);
    completed_results_queue_release(&out);

    assert(completed_results_queue_pop_any(&out));
    assert(out.subsystem == COMPLETED_SUBSYSTEM_DIAGNOSTICS);
    assert(out.seq == 3);
    completed_results_queue_release(&out);

    assert(!completed_results_queue_pop_any(&out));
}

static void test_owned_payload_release_on_pop(void) {
    completed_results_queue_reset();
    g_freed_owned_payloads = 0;

    CompletedResult result;
    memset(&result, 0, sizeof(result));
    result.subsystem = COMPLETED_SUBSYSTEM_ANALYSIS;
    result.kind = COMPLETED_RESULT_ANALYSIS_STATUS_UPDATE;
    result.owned_ptr = malloc(8);
    assert(result.owned_ptr != NULL);
    result.free_owned_ptr = test_owned_payload_free;
    assert(completed_results_queue_push(&result));

    CompletedResult out;
    assert(completed_results_queue_pop_any(&out));
    assert(g_freed_owned_payloads == 0);
    completed_results_queue_release(&out);
    assert(g_freed_owned_payloads == 1);
}

static void test_owned_payload_release_on_reset(void) {
    completed_results_queue_reset();
    g_freed_owned_payloads = 0;

    CompletedResult result;
    memset(&result, 0, sizeof(result));
    result.subsystem = COMPLETED_SUBSYSTEM_SYMBOLS;
    result.kind = COMPLETED_RESULT_SYMBOLS_UPDATED;
    result.owned_ptr = malloc(8);
    assert(result.owned_ptr != NULL);
    result.free_owned_ptr = test_owned_payload_free;
    assert(completed_results_queue_push(&result));

    completed_results_queue_reset();
    assert(g_freed_owned_payloads == 1);
}

static void test_stats_tracking(void) {
    completed_results_queue_reset();
    CompletedResultsQueueStats stats;
    memset(&stats, 0, sizeof(stats));

    push_basic_result(COMPLETED_SUBSYSTEM_ANALYSIS, COMPLETED_RESULT_ANALYSIS_PROGRESS, 1);
    push_basic_result(COMPLETED_SUBSYSTEM_SYMBOLS, COMPLETED_RESULT_SYMBOLS_UPDATED, 2);
    completed_results_queue_snapshot(&stats);
    assert(stats.pushed == 2);
    assert(stats.popped == 0);
    assert(stats.total_depth == 2);
    assert(stats.analysis_depth == 1);
    assert(stats.symbols_depth == 1);
    assert(stats.high_watermark >= 2);

    CompletedResult out;
    assert(completed_results_queue_pop_any(&out));
    completed_results_queue_release(&out);
    completed_results_queue_note_applied();
    completed_results_queue_note_stale_dropped();

    completed_results_queue_snapshot(&stats);
    assert(stats.popped == 1);
    assert(stats.total_depth <= 1);
    assert(stats.results_applied == 1);
    assert(stats.results_stale_dropped == 1);

    // Ensure staged-head entries are still consumable after pop_any prefetch.
    assert(completed_results_queue_pop_any(&out));
    completed_results_queue_release(&out);
}

static void test_push_failure_releases_owned_payload(void) {
    completed_results_queue_reset();
    int pushed = fill_subsystem_until_full(COMPLETED_SUBSYSTEM_ANALYSIS);
    assert(pushed > 0);

    g_freed_owned_payloads = 0;
    CompletedResult result;
    memset(&result, 0, sizeof(result));
    result.subsystem = COMPLETED_SUBSYSTEM_ANALYSIS;
    result.kind = COMPLETED_RESULT_ANALYSIS_STATUS_UPDATE;
    result.owned_ptr = malloc(8);
    assert(result.owned_ptr != NULL);
    result.free_owned_ptr = test_owned_payload_free;

    assert(!completed_results_queue_push(&result));
    assert(g_freed_owned_payloads == 1);
    completed_results_queue_reset();
}

int main(void) {
    completed_results_queue_init();

    test_pop_any_global_ordering();
    test_owned_payload_release_on_pop();
    test_owned_payload_release_on_reset();
    test_stats_tracking();
    test_push_failure_releases_owned_payload();

    completed_results_queue_shutdown();
    puts("completed_results_queue_test: success");
    return 0;
}
