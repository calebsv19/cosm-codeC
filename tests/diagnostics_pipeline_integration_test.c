#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "app/GlobalInfo/core_state.h"
#include "core/Analysis/analysis_store.h"
#include "core/LoopEvents/event_invalidation_policy.h"
#include "core/LoopEvents/event_queue.h"
#include "core/LoopResults/completed_results_queue.h"
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

static void reset_pane_dirty(UIPane* pane) {
    if (!pane) return;
    pane->dirty = false;
    pane->dirtyReasons = PANE_INVALIDATION_NONE;
}

static bool apply_diagnostics_result_like_main_loop(const CompletedResult* result,
                                                    const char* project_root) {
    if (!result || result->kind != COMPLETED_RESULT_DIAGNOSTICS_UPDATED) return false;
    const CompletedResultDiagnosticsUpdatedPayload* p = &result->payload.diagnostics_updated;
    if (p->project_root[0] && strcmp(p->project_root, project_root) != 0) {
        completed_results_queue_note_stale_dropped();
        return false;
    }
    if (analysis_store_combined_stamp() != p->diagnostics_stamp) {
        completed_results_queue_note_stale_dropped();
        return false;
    }
    analysis_store_flatten_to_engine();
    completed_results_queue_note_applied();
    return loop_events_emit_diagnostics_updated(p->project_root,
                                                p->analysis_run_id,
                                                p->diagnostics_stamp);
}

static void dispatch_visitor(const IDEEvent* event, void* user_data) {
    LoopEventInvalidationDispatchTargets* targets =
        (LoopEventInvalidationDispatchTargets*)user_data;
    if (event && event->type == IDE_EVENT_DIAGNOSTICS_UPDATED) {
        analysis_store_mark_published(event->payload.analysis.data_stamp);
    }
    loop_events_dispatch_invalidation(event, targets);
}

static void seed_store_single_diag(const char* file_path, const char* message) {
    FisicsDiagnostic d = {0};
    d.file_path = file_path;
    d.line = 2;
    d.column = 1;
    d.length = 1;
    d.kind = DIAG_WARNING;
    d.message = (char*)message;
    analysis_store_upsert(file_path, &d, 1u);
}

static void test_diagnostics_apply_dispatch_and_invalidation(void) {
    const char* project_root = "/tmp/diagnostics_pipeline_integration";
    const char* file_path = "/tmp/diagnostics_pipeline_integration/src/a.c";

    completed_results_queue_reset();
    analysis_store_clear();
    loop_events_reset();
    analysis_store_mark_published(0u);

    seed_store_single_diag(file_path, "diag one");
    uint64_t stamp = analysis_store_combined_stamp();
    assert(stamp > 0u);

    CompletedResult in;
    memset(&in, 0, sizeof(in));
    in.subsystem = COMPLETED_SUBSYSTEM_DIAGNOSTICS;
    in.kind = COMPLETED_RESULT_DIAGNOSTICS_UPDATED;
    in.payload.diagnostics_updated.analysis_run_id = 77u;
    in.payload.diagnostics_updated.diagnostics_stamp = stamp;
    snprintf(in.payload.diagnostics_updated.project_root,
             sizeof(in.payload.diagnostics_updated.project_root),
             "%s",
             project_root);
    assert(completed_results_queue_push(&in));

    CompletedResult popped;
    memset(&popped, 0, sizeof(popped));
    assert(completed_results_queue_pop_any(&popped));
    assert(apply_diagnostics_result_like_main_loop(&popped, project_root));
    completed_results_queue_release(&popped);

    UIPane editor = {0};
    UIPane control = {0};
    UIPane tool = {0};
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
    size_t drained = loop_events_drain_bounded(16u, dispatch_visitor, &targets);
    assert(drained == 1u);

    assert(analysis_store_published_stamp() == stamp);
    assert(editor.dirty);
    assert(!control.dirty);
    assert(tool.dirty);
    assert((editor.dirtyReasons & PANE_INVALIDATION_CONTENT) != 0u);
    assert((tool.dirtyReasons & PANE_INVALIDATION_BACKGROUND) != 0u);
}

static void test_stale_diagnostics_result_is_dropped(void) {
    const char* project_root = "/tmp/diagnostics_pipeline_integration";
    const char* file_path = "/tmp/diagnostics_pipeline_integration/src/a.c";

    completed_results_queue_reset();
    analysis_store_clear();
    loop_events_reset();
    analysis_store_mark_published(0u);

    seed_store_single_diag(file_path, "diag one");
    uint64_t stale_stamp = analysis_store_combined_stamp();
    seed_store_single_diag(file_path, "diag two");
    uint64_t live_stamp = analysis_store_combined_stamp();
    assert(live_stamp > stale_stamp);

    CompletedResult in;
    memset(&in, 0, sizeof(in));
    in.subsystem = COMPLETED_SUBSYSTEM_DIAGNOSTICS;
    in.kind = COMPLETED_RESULT_DIAGNOSTICS_UPDATED;
    in.payload.diagnostics_updated.analysis_run_id = 88u;
    in.payload.diagnostics_updated.diagnostics_stamp = stale_stamp;
    snprintf(in.payload.diagnostics_updated.project_root,
             sizeof(in.payload.diagnostics_updated.project_root),
             "%s",
             project_root);
    assert(completed_results_queue_push(&in));

    CompletedResult popped;
    memset(&popped, 0, sizeof(popped));
    assert(completed_results_queue_pop_any(&popped));
    assert(!apply_diagnostics_result_like_main_loop(&popped, project_root));
    completed_results_queue_release(&popped);

    IDEEvent ev = {0};
    assert(!loop_events_pop(&ev));
    assert(analysis_store_published_stamp() == 0u);
}

int main(void) {
    completed_results_queue_init();
    loop_events_init();

    test_diagnostics_apply_dispatch_and_invalidation();
    test_stale_diagnostics_result_is_dropped();

    loop_events_shutdown();
    completed_results_queue_shutdown();
    analysis_store_clear();
    puts("diagnostics_pipeline_integration_test: success");
    return 0;
}
