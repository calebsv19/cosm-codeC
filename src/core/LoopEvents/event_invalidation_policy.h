#ifndef EVENT_INVALIDATION_POLICY_H
#define EVENT_INVALIDATION_POLICY_H

#include <stdbool.h>
#include <stdint.h>

#include "core/LoopEvents/event_queue.h"
#include "ide/Panes/PaneInfo/pane.h"

typedef enum LoopEventInvalidationIntent {
    LOOP_EVENT_INVALIDATE_NONE = 0,
    LOOP_EVENT_INVALIDATE_CONTENT = 1,
    LOOP_EVENT_INVALIDATE_CONTENT_BACKGROUND = 2,
    LOOP_EVENT_INVALIDATE_CONTENT_INPUT = 3
} LoopEventInvalidationIntent;

typedef enum LoopEventInvalidationTarget {
    LOOP_EVENT_TARGET_NONE = 0,
    LOOP_EVENT_TARGET_EDITOR = 1u << 0,
    LOOP_EVENT_TARGET_CONTROL = 1u << 1,
    LOOP_EVENT_TARGET_TOOL = 1u << 2,
    LOOP_EVENT_TARGET_ALL = 1u << 3
} LoopEventInvalidationTarget;

typedef struct LoopEventInvalidationPlan {
    uint32_t targets;
    LoopEventInvalidationIntent invalidate_intent;
    LoopEventInvalidationIntent redraw_intent;
} LoopEventInvalidationPlan;

typedef struct LoopEventInvalidationDispatchTargets {
    struct UIPane* editor_pane;
    struct UIPane* control_pane;
    struct UIPane* tool_pane;
    struct UIPane** all_panes;
    int all_pane_count;
} LoopEventInvalidationDispatchTargets;

bool loop_events_build_invalidation_plan(const IDEEvent* event, LoopEventInvalidationPlan* out);
bool loop_events_dispatch_invalidation(const IDEEvent* event,
                                       const LoopEventInvalidationDispatchTargets* targets);

#endif // EVENT_INVALIDATION_POLICY_H
