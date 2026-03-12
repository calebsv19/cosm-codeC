#include "core/LoopEvents/event_invalidation_policy.h"

#include "app/GlobalInfo/core_state.h"
#include <string.h>

bool loop_events_build_invalidation_plan(const IDEEvent* event, LoopEventInvalidationPlan* out) {
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    if (!event) return false;

    switch (event->type) {
        case IDE_EVENT_DOCUMENT_EDITED:
        case IDE_EVENT_DOCUMENT_REVISION_CHANGED:
            out->targets = LOOP_EVENT_TARGET_EDITOR;
            out->invalidate_intent = LOOP_EVENT_INVALIDATE_CONTENT_INPUT;
            out->redraw_intent = LOOP_EVENT_INVALIDATE_CONTENT;
            return true;
        case IDE_EVENT_SYMBOL_TREE_UPDATED:
            out->targets = LOOP_EVENT_TARGET_CONTROL | LOOP_EVENT_TARGET_EDITOR;
            out->invalidate_intent = LOOP_EVENT_INVALIDATE_CONTENT_BACKGROUND;
            out->redraw_intent = LOOP_EVENT_INVALIDATE_CONTENT_BACKGROUND;
            return true;
        case IDE_EVENT_DIAGNOSTICS_UPDATED:
            out->targets = LOOP_EVENT_TARGET_CONTROL | LOOP_EVENT_TARGET_TOOL | LOOP_EVENT_TARGET_EDITOR;
            out->invalidate_intent = LOOP_EVENT_INVALIDATE_CONTENT_BACKGROUND;
            out->redraw_intent = LOOP_EVENT_INVALIDATE_CONTENT_BACKGROUND;
            return true;
        case IDE_EVENT_NONE:
        default:
            return false;
    }
}

static unsigned int invalidation_flags_from_intent(LoopEventInvalidationIntent intent) {
    switch (intent) {
        case LOOP_EVENT_INVALIDATE_CONTENT:
            return RENDER_INVALIDATION_CONTENT;
        case LOOP_EVENT_INVALIDATE_CONTENT_BACKGROUND:
            return RENDER_INVALIDATION_CONTENT | RENDER_INVALIDATION_BACKGROUND;
        case LOOP_EVENT_INVALIDATE_CONTENT_INPUT:
            return RENDER_INVALIDATION_CONTENT | RENDER_INVALIDATION_INPUT;
        case LOOP_EVENT_INVALIDATE_NONE:
        default:
            return 0u;
    }
}

bool loop_events_dispatch_invalidation(const IDEEvent* event,
                                       const LoopEventInvalidationDispatchTargets* targets) {
    if (!targets) return false;
    LoopEventInvalidationPlan plan;
    if (!loop_events_build_invalidation_plan(event, &plan)) return false;

    unsigned int invalidate_flags = invalidation_flags_from_intent(plan.invalidate_intent);
    unsigned int redraw_flags = invalidation_flags_from_intent(plan.redraw_intent);
    if (invalidate_flags == 0u && redraw_flags == 0u) return false;

    if ((plan.targets & LOOP_EVENT_TARGET_EDITOR) && targets->editor_pane) {
        invalidatePane(targets->editor_pane, invalidate_flags);
    }
    if ((plan.targets & LOOP_EVENT_TARGET_CONTROL) && targets->control_pane) {
        invalidatePane(targets->control_pane, invalidate_flags);
    }
    if ((plan.targets & LOOP_EVENT_TARGET_TOOL) && targets->tool_pane) {
        invalidatePane(targets->tool_pane, invalidate_flags);
    }
    if (plan.targets == LOOP_EVENT_TARGET_NONE || (plan.targets & LOOP_EVENT_TARGET_ALL)) {
        if (targets->all_panes && targets->all_pane_count > 0) {
            invalidateAll(targets->all_panes, targets->all_pane_count, invalidate_flags);
        }
    }
    if (redraw_flags != 0u) {
        requestFullRedraw(redraw_flags);
    }
    return true;
}
