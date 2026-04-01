#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "app/GlobalInfo/core_state.h"
#include "core/LoopEvents/event_invalidation_policy.h"
#include "core/LoopEvents/event_queue.h"
#include "ide/Panes/PaneInfo/pane.h"

static bool g_frame_invalidated = false;
static bool g_full_redraw = false;
static uint32_t g_reasons = 0u;

void invalidatePane(struct UIPane* pane, uint32_t reasonBits) {
    if (!pane || reasonBits == RENDER_INVALIDATION_NONE) return;
    pane->dirty = true;
    pane->dirtyReasons |= reasonBits;
    g_frame_invalidated = true;
    g_reasons |= reasonBits;
}

void invalidateAll(struct UIPane** panes, int paneCount, uint32_t reasonBits) {
    if (!panes || paneCount <= 0) return;
    for (int i = 0; i < paneCount; ++i) {
        invalidatePane(panes[i], reasonBits);
    }
}

void requestFullRedraw(uint32_t reasonBits) {
    if (reasonBits == RENDER_INVALIDATION_NONE) return;
    g_frame_invalidated = true;
    g_full_redraw = true;
    g_reasons |= reasonBits;
}

static void reset_frame_state(void) {
    g_frame_invalidated = false;
    g_full_redraw = false;
    g_reasons = 0u;
}

static void dispatch_visitor(const IDEEvent* event, void* user_data) {
    LoopEventInvalidationDispatchTargets* targets =
        (LoopEventInvalidationDispatchTargets*)user_data;
    loop_events_dispatch_invalidation(event, targets);
}

static void reset_pane_dirty(UIPane* pane) {
    if (!pane) return;
    pane->dirty = false;
    pane->dirtyReasons = PANE_INVALIDATION_NONE;
}

static void test_event_queue_to_invalidation_dispatch(void) {
    loop_events_init();

    UIPane editor;
    UIPane control;
    UIPane tool;
    memset(&editor, 0, sizeof(editor));
    memset(&control, 0, sizeof(control));
    memset(&tool, 0, sizeof(tool));
    editor.role = PANE_ROLE_EDITOR;
    control.role = PANE_ROLE_CONTROLPANEL;
    tool.role = PANE_ROLE_TOOLPANEL;

    reset_pane_dirty(&editor);
    reset_pane_dirty(&control);
    reset_pane_dirty(&tool);
    reset_frame_state();

    UIPane* all_panes[3] = { &editor, &control, &tool };
    LoopEventInvalidationDispatchTargets targets = {
        .editor_pane = &editor,
        .control_pane = &control,
        .tool_pane = &tool,
        .all_panes = all_panes,
        .all_pane_count = 3
    };

    assert(loop_events_emit_symbol_tree_updated("/tmp/project", 1u, 11u));
    assert(loop_events_emit_diagnostics_updated("/tmp/project", 2u, 22u));

    size_t drained = loop_events_drain_bounded(64u, dispatch_visitor, &targets);
    assert(drained == 2u);

    assert(editor.dirty);
    assert(control.dirty);
    assert(tool.dirty);
    assert((editor.dirtyReasons & PANE_INVALIDATION_CONTENT) != 0u);
    assert((control.dirtyReasons & PANE_INVALIDATION_BACKGROUND) != 0u);
    assert((tool.dirtyReasons & PANE_INVALIDATION_BACKGROUND) != 0u);

    assert(g_frame_invalidated);
    assert((g_reasons & RENDER_INVALIDATION_CONTENT) != 0u);
    assert((g_reasons & RENDER_INVALIDATION_BACKGROUND) != 0u);

    // Reset pane state for document edit path check.
    reset_pane_dirty(&editor);
    reset_pane_dirty(&control);
    reset_pane_dirty(&tool);
    reset_frame_state();

    assert(loop_events_emit_document_edited("src/main.c", 9u));
    drained = loop_events_drain_bounded(64u, dispatch_visitor, &targets);
    assert(drained == 1u);

    assert(editor.dirty);
    assert((editor.dirtyReasons & PANE_INVALIDATION_INPUT) != 0u);
    assert(!control.dirty);
    assert(!tool.dirty);

    // Analysis status updates should target control only.
    reset_pane_dirty(&editor);
    reset_pane_dirty(&control);
    reset_pane_dirty(&tool);
    reset_frame_state();
    assert(loop_events_emit_analysis_status_updated("/tmp/project", 3u, 33u));
    drained = loop_events_drain_bounded(64u, dispatch_visitor, &targets);
    assert(drained == 1u);
    assert(!editor.dirty);
    assert(control.dirty);
    assert(!tool.dirty);

    loop_events_shutdown();
}

int main(void) {
    test_event_queue_to_invalidation_dispatch();
    puts("loop_events_dispatch_integration_test: success");
    return 0;
}
