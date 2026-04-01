#include "core/LoopEvents/event_invalidation_policy.h"

#include "app/GlobalInfo/core_state.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static bool event_invalidation_log_enabled(void) {
    static bool initialized = false;
    static bool enabled = false;
    if (!initialized) {
        const char* env = getenv("IDE_EVENT_INVALIDATION_LOG");
        enabled = (env && env[0] &&
                   (strcmp(env, "1") == 0 || strcasecmp(env, "true") == 0));
        initialized = true;
    }
    return enabled;
}

static const char* event_type_name(IDEEventType type) {
    switch (type) {
        case IDE_EVENT_DOCUMENT_EDITED: return "document_edited";
        case IDE_EVENT_DOCUMENT_REVISION_CHANGED: return "document_revision_changed";
        case IDE_EVENT_SYMBOL_TREE_UPDATED: return "symbol_tree_updated";
        case IDE_EVENT_DIAGNOSTICS_UPDATED: return "diagnostics_updated";
        case IDE_EVENT_ANALYSIS_PROGRESS_UPDATED: return "analysis_progress_updated";
        case IDE_EVENT_ANALYSIS_STATUS_UPDATED: return "analysis_status_updated";
        case IDE_EVENT_LIBRARY_INDEX_UPDATED: return "library_index_updated";
        case IDE_EVENT_ANALYSIS_RUN_FINISHED: return "analysis_run_finished";
        case IDE_EVENT_NONE:
        default: return "none";
    }
}

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
            out->targets = LOOP_EVENT_TARGET_TOOL | LOOP_EVENT_TARGET_EDITOR;
            out->invalidate_intent = LOOP_EVENT_INVALIDATE_CONTENT_BACKGROUND;
            out->redraw_intent = LOOP_EVENT_INVALIDATE_CONTENT_BACKGROUND;
            return true;
        case IDE_EVENT_ANALYSIS_PROGRESS_UPDATED:
        case IDE_EVENT_ANALYSIS_STATUS_UPDATED:
            out->targets = LOOP_EVENT_TARGET_CONTROL;
            out->invalidate_intent = LOOP_EVENT_INVALIDATE_CONTENT_BACKGROUND;
            out->redraw_intent = LOOP_EVENT_INVALIDATE_CONTENT_BACKGROUND;
            return true;
        case IDE_EVENT_LIBRARY_INDEX_UPDATED:
            out->targets = LOOP_EVENT_TARGET_TOOL;
            out->invalidate_intent = LOOP_EVENT_INVALIDATE_CONTENT_BACKGROUND;
            out->redraw_intent = LOOP_EVENT_INVALIDATE_CONTENT_BACKGROUND;
            return true;
        case IDE_EVENT_ANALYSIS_RUN_FINISHED:
            out->targets = LOOP_EVENT_TARGET_CONTROL;
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

    if (event_invalidation_log_enabled() && event) {
        fprintf(stderr,
                "[EventInvalidation] type=%s targets=0x%x invalidate=0x%x redraw=0x%x\n",
                event_type_name(event->type),
                (unsigned int)plan.targets,
                invalidate_flags,
                redraw_flags);
    }

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
