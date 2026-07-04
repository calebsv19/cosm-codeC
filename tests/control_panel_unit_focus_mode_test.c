#include "ide/Panes/ControlPanel/control_panel_internal.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static ControlPanelControllerState g_state;
static int g_tree_dirty_count = 0;
static int g_visible_refresh_count = 0;
static int g_projection_sync_count = 0;

ControlPanelControllerState* control_panel_state(void) {
    return &g_state;
}

void control_panel_mark_visible_tree_dirty(void) {
    g_tree_dirty_count++;
}

void control_panel_refresh_visible_symbol_tree(void) {
    g_visible_refresh_count++;
}

void editor_sync_active_file_projection_mode(void) {
    g_projection_sync_count++;
}

int main(void) {
    memset(&g_state, 0, sizeof(g_state));
    g_state.filters.target_symbols_enabled = true;
    g_state.filters.target_units_enabled = false;
    g_state.filters.target_editor_enabled = false;
    g_state.filters.search_scope = CONTROL_SEARCH_SCOPE_PROJECT_FILES;
    g_state.filters.editor_view_mode = CONTROL_EDITOR_VIEW_PROJECTION;
    g_state.filters.unit_dimension_mask = CONTROL_UNIT_DIM_TIME | CONTROL_UNIT_DIM_MASS;
    g_state.ui.search_focused = true;

    assert(!control_panel_focus_unit_marker_query(NULL));
    assert(!control_panel_focus_unit_marker_query(""));

    assert(control_panel_focus_unit_marker_query("velocity_x"));
    assert(control_panel_is_search_enabled());
    assert(control_panel_target_units_enabled());
    assert(!control_panel_target_symbols_enabled());
    assert(control_panel_target_editor_enabled());
    assert(control_panel_get_search_scope() == CONTROL_SEARCH_SCOPE_PROJECT_FILES);
    assert(control_panel_get_editor_view_mode() == CONTROL_EDITOR_VIEW_MARKERS);
    assert(control_panel_get_unit_dimension_mask() == 0u);
    assert(strcmp(control_panel_get_search_query(), "velocity_x") == 0);
    assert(control_panel_get_search_cursor() == (int)strlen("velocity_x"));
    assert(!control_panel_is_search_focused());

    ControlPanelProjectionOptions projectionOptions;
    control_panel_capture_projection_options(&projectionOptions);
    assert(projectionOptions.query == control_panel_get_search_query());
    assert(projectionOptions.search_enabled);
    assert(projectionOptions.query_has_text);
    assert(projectionOptions.query_active);
    assert(!projectionOptions.live_parse_enabled);
    assert(!projectionOptions.inline_errors_enabled);
    assert(!projectionOptions.macros_enabled);
    assert(!projectionOptions.target_symbols_enabled);
    assert(projectionOptions.target_units_enabled);
    assert(projectionOptions.target_editor_enabled);
    assert(projectionOptions.search_scope == CONTROL_SEARCH_SCOPE_PROJECT_FILES);
    assert(projectionOptions.scope_project_files);
    assert(projectionOptions.editor_view_mode == CONTROL_EDITOR_VIEW_MARKERS);
    assert(!projectionOptions.projection_render_enabled);
    assert(projectionOptions.marker_render_enabled);
    assert(projectionOptions.unit_dimension_mask == 0u);
    assert(projectionOptions.symbol_filter_options.scope == SYMBOL_FILTER_SCOPE_PROJECT);
    assert(g_tree_dirty_count == 1);
    assert(g_visible_refresh_count == 0);
    assert(g_projection_sync_count == 1);

    printf("control_panel_unit_focus_mode_test: ok\n");
    return 0;
}
