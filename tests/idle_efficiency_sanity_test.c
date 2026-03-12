#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/LoopEvents/event_queue.h"
#include "core/LoopResults/completed_results_queue.h"

static void test_idle_snapshots_stay_stable(void) {
    loop_events_init();
    completed_results_queue_init();

    LoopEventsStats events_before;
    LoopEventsStats events_after;
    CompletedResultsQueueStats results_before;
    CompletedResultsQueueStats results_after;
    memset(&events_before, 0, sizeof(events_before));
    memset(&events_after, 0, sizeof(events_after));
    memset(&results_before, 0, sizeof(results_before));
    memset(&results_after, 0, sizeof(results_after));

    loop_events_snapshot(&events_before);
    completed_results_queue_snapshot(&results_before);

    for (int i = 0; i < 200; ++i) {
        // Synthetic idle tick: no inputs, no worker results, no event injections.
        assert(loop_events_drain_bounded(64u, NULL, NULL) == 0u);

        CompletedResult out;
        memset(&out, 0, sizeof(out));
        assert(!completed_results_queue_pop_any(&out));
    }

    loop_events_snapshot(&events_after);
    completed_results_queue_snapshot(&results_after);

    assert(events_before.depth == 0u);
    assert(events_after.depth == 0u);
    assert(events_before.events_enqueued == events_after.events_enqueued);
    assert(events_before.events_processed == events_after.events_processed);
    assert(events_before.events_deferred == events_after.events_deferred);
    assert(events_before.events_dropped_overflow == events_after.events_dropped_overflow);

    assert(results_before.total_depth == 0u);
    assert(results_after.total_depth == 0u);
    assert(results_before.pushed == results_after.pushed);
    assert(results_before.popped == results_after.popped);
    assert(results_before.results_applied == results_after.results_applied);
    assert(results_before.results_stale_dropped == results_after.results_stale_dropped);

    completed_results_queue_shutdown();
    loop_events_shutdown();
}

int main(void) {
    test_idle_snapshots_stay_stable();
    puts("idle_efficiency_sanity_test: success");
    return 0;
}
