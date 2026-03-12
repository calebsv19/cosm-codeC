#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/LoopEvents/event_queue.h"

static IDEEvent make_doc_event(IDEEventType type, const char* path, uint64_t revision) {
    IDEEvent event;
    memset(&event, 0, sizeof(event));
    event.type = type;
    if (path) {
        strncpy(event.payload.document.document_path,
                path,
                sizeof(event.payload.document.document_path) - 1u);
    }
    event.payload.document.document_revision = revision;
    return event;
}

typedef struct DrainVisitState {
    uint64_t seen_sequence_sum;
    uint32_t visited;
} DrainVisitState;

static void visit_and_count(const IDEEvent* event, void* user_data) {
    if (!event || !user_data) return;
    DrainVisitState* state = (DrainVisitState*)user_data;
    state->visited++;
    state->seen_sequence_sum += event->sequence;
}

static void test_fifo_and_sequence(void) {
    loop_events_init();

    IDEEvent ev1 = make_doc_event(IDE_EVENT_DOCUMENT_EDITED, "a.c", 2u);
    IDEEvent ev2 = make_doc_event(IDE_EVENT_DOCUMENT_REVISION_CHANGED, "a.c", 3u);
    IDEEvent ev3 = make_doc_event(IDE_EVENT_DOCUMENT_EDITED, "b.c", 7u);

    assert(loop_events_push(&ev1));
    assert(loop_events_push(&ev2));
    assert(loop_events_push(&ev3));
    assert(loop_events_size() == 3u);

    IDEEvent popped;
    memset(&popped, 0, sizeof(popped));

    assert(loop_events_pop(&popped));
    assert(popped.type == IDE_EVENT_DOCUMENT_EDITED);
    assert(strcmp(popped.payload.document.document_path, "a.c") == 0);
    assert(popped.payload.document.document_revision == 2u);
    assert(popped.sequence == 1u);

    assert(loop_events_pop(&popped));
    assert(popped.type == IDE_EVENT_DOCUMENT_REVISION_CHANGED);
    assert(popped.payload.document.document_revision == 3u);
    assert(popped.sequence == 2u);

    assert(loop_events_pop(&popped));
    assert(popped.type == IDE_EVENT_DOCUMENT_EDITED);
    assert(strcmp(popped.payload.document.document_path, "b.c") == 0);
    assert(popped.payload.document.document_revision == 7u);
    assert(popped.sequence == 3u);

    assert(!loop_events_pop(&popped));

    LoopEventsStats stats;
    memset(&stats, 0, sizeof(stats));
    loop_events_snapshot(&stats);
    assert(stats.events_enqueued == 3u);
    assert(stats.events_processed == 3u);
    assert(stats.events_dropped_overflow == 0u);

    loop_events_shutdown();
}

static void test_overflow_and_deferred_counters(void) {
    loop_events_init();

    const size_t cap = loop_events_capacity();
    IDEEvent doc_event = make_doc_event(IDE_EVENT_DOCUMENT_EDITED, "overflow.c", 1u);
    for (size_t i = 0; i < cap; ++i) {
        assert(loop_events_push(&doc_event));
    }

    assert(!loop_events_push(&doc_event));
    loop_events_note_deferred(11u);

    LoopEventsStats stats;
    memset(&stats, 0, sizeof(stats));
    loop_events_snapshot(&stats);
    assert(stats.depth == cap);
    assert(stats.high_watermark == cap);
    assert(stats.events_enqueued == cap);
    assert(stats.events_dropped_overflow == 1u);
    assert(stats.events_deferred == 11u);

    loop_events_shutdown();
}

static void test_drain_bounded_and_deferred(void) {
    loop_events_init();

    IDEEvent event = make_doc_event(IDE_EVENT_DOCUMENT_EDITED, "budget.c", 1u);
    for (int i = 0; i < 5; ++i) {
        assert(loop_events_push(&event));
    }

    DrainVisitState state;
    memset(&state, 0, sizeof(state));
    size_t drained = loop_events_drain_bounded(2u, visit_and_count, &state);
    assert(drained == 2u);
    assert(state.visited == 2u);
    assert(state.seen_sequence_sum == 3u); // seq 1 + 2

    LoopEventsStats stats;
    memset(&stats, 0, sizeof(stats));
    loop_events_snapshot(&stats);
    assert(stats.events_processed == 2u);
    assert(stats.events_deferred == 3u);
    assert(stats.depth == 3u);

    memset(&state, 0, sizeof(state));
    drained = loop_events_drain_bounded(128u, visit_and_count, &state);
    assert(drained == 3u);
    assert(state.visited == 3u);
    assert(state.seen_sequence_sum == 12u); // seq 3 + 4 + 5

    memset(&stats, 0, sizeof(stats));
    loop_events_snapshot(&stats);
    assert(stats.events_processed == 5u);
    assert(stats.depth == 0u);

    loop_events_shutdown();
}

int main(void) {
    test_fifo_and_sequence();
    test_overflow_and_deferred_counters();
    test_drain_bounded_and_deferred();
    puts("loop_events_queue_test: success");
    return 0;
}
