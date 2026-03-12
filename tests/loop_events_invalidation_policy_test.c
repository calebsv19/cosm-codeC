#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/LoopEvents/event_invalidation_policy.h"

static IDEEvent make_event(IDEEventType type) {
    IDEEvent event;
    memset(&event, 0, sizeof(event));
    event.type = type;
    return event;
}

static void test_document_mapping(void) {
    LoopEventInvalidationPlan plan;
    IDEEvent event = make_event(IDE_EVENT_DOCUMENT_EDITED);
    assert(loop_events_build_invalidation_plan(&event, &plan));
    assert(plan.targets == LOOP_EVENT_TARGET_EDITOR);
    assert(plan.invalidate_intent == LOOP_EVENT_INVALIDATE_CONTENT_INPUT);
    assert(plan.redraw_intent == LOOP_EVENT_INVALIDATE_CONTENT);

    event = make_event(IDE_EVENT_DOCUMENT_REVISION_CHANGED);
    assert(loop_events_build_invalidation_plan(&event, &plan));
    assert(plan.targets == LOOP_EVENT_TARGET_EDITOR);
}

static void test_symbol_diagnostics_mapping(void) {
    LoopEventInvalidationPlan plan;

    IDEEvent event = make_event(IDE_EVENT_SYMBOL_TREE_UPDATED);
    assert(loop_events_build_invalidation_plan(&event, &plan));
    assert(plan.targets == (LOOP_EVENT_TARGET_CONTROL | LOOP_EVENT_TARGET_EDITOR));
    assert(plan.invalidate_intent == LOOP_EVENT_INVALIDATE_CONTENT_BACKGROUND);
    assert(plan.redraw_intent == LOOP_EVENT_INVALIDATE_CONTENT_BACKGROUND);

    event = make_event(IDE_EVENT_DIAGNOSTICS_UPDATED);
    assert(loop_events_build_invalidation_plan(&event, &plan));
    assert(plan.targets == (LOOP_EVENT_TARGET_CONTROL | LOOP_EVENT_TARGET_TOOL | LOOP_EVENT_TARGET_EDITOR));
    assert(plan.invalidate_intent == LOOP_EVENT_INVALIDATE_CONTENT_BACKGROUND);
    assert(plan.redraw_intent == LOOP_EVENT_INVALIDATE_CONTENT_BACKGROUND);

    event = make_event(IDE_EVENT_ANALYSIS_PROGRESS_UPDATED);
    assert(loop_events_build_invalidation_plan(&event, &plan));
    assert(plan.targets == (LOOP_EVENT_TARGET_CONTROL | LOOP_EVENT_TARGET_TOOL));

    event = make_event(IDE_EVENT_ANALYSIS_STATUS_UPDATED);
    assert(loop_events_build_invalidation_plan(&event, &plan));
    assert(plan.targets == (LOOP_EVENT_TARGET_CONTROL | LOOP_EVENT_TARGET_TOOL));

    event = make_event(IDE_EVENT_LIBRARY_INDEX_UPDATED);
    assert(loop_events_build_invalidation_plan(&event, &plan));
    assert(plan.targets == (LOOP_EVENT_TARGET_CONTROL | LOOP_EVENT_TARGET_TOOL));

    event = make_event(IDE_EVENT_ANALYSIS_RUN_FINISHED);
    assert(loop_events_build_invalidation_plan(&event, &plan));
    assert(plan.targets == (LOOP_EVENT_TARGET_CONTROL | LOOP_EVENT_TARGET_TOOL));
}

static void test_none_mapping(void) {
    LoopEventInvalidationPlan plan;
    IDEEvent event = make_event(IDE_EVENT_NONE);
    assert(!loop_events_build_invalidation_plan(&event, &plan));
}

int main(void) {
    test_document_mapping();
    test_symbol_diagnostics_mapping();
    test_none_mapping();
    puts("loop_events_invalidation_policy_test: success");
    return 0;
}
