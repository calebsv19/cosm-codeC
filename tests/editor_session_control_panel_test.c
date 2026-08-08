#include "ide/Panes/Editor/editor_session_control_panel.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static ControlPanelPersistState sample_state(void) {
    ControlPanelPersistState state;
    memset(&state, 0, sizeof(state));
    state.search_enabled = true;
    snprintf(state.search_query, sizeof(state.search_query), "%s", "mass");
    state.filters_collapsed = true;
    state.target_symbols_enabled = true;
    state.target_units_enabled = true;
    state.target_editor_enabled = true;
    state.search_scope = CONTROL_SEARCH_SCOPE_PROJECT_FILES;
    state.match_all_enabled = false;
    state.match_methods_enabled = true;
    state.match_types_enabled = false;
    state.match_vars_enabled = true;
    state.match_tags_enabled = false;
    state.match_order[0] = CONTROL_FILTER_BTN_MATCH_VARS;
    state.match_order[1] = CONTROL_FILTER_BTN_MATCH_METHODS;
    state.match_order[2] = CONTROL_FILTER_BTN_MATCH_TYPES;
    state.match_order[3] = CONTROL_FILTER_BTN_MATCH_TAGS;
    state.editor_view_mode = CONTROL_EDITOR_VIEW_MARKERS;
    state.field_name = true;
    state.field_type = true;
    state.field_params = false;
    state.field_kind = true;
    state.unit_dimension_mask = CONTROL_UNIT_DIM_MASS | CONTROL_UNIT_DIM_FORCE;
    state.live_parse_enabled = false;
    state.inline_errors_enabled = true;
    state.macros_enabled = true;
    return state;
}

int main(void) {
    ControlPanelPersistState original = sample_state();
    json_object* control = editor_session_control_panel_serialize(&original);
    assert(control);

    json_object* field = NULL;
    assert(json_object_object_get_ex(control, "target_units_enabled", &field));
    assert(json_object_get_boolean(field));
    assert(json_object_object_get_ex(control, "unit_dimension_mask", &field));
    assert((unsigned int)json_object_get_int(field) ==
           (CONTROL_UNIT_DIM_MASS | CONTROL_UNIT_DIM_FORCE));

    json_object* payload = json_object_new_object();
    json_object_object_add(payload, "control_panel", control);

    ControlPanelPersistState restored;
    memset(&restored, 0, sizeof(restored));
    restored.target_units_enabled = false;
    restored.unit_dimension_mask = CONTROL_UNIT_DIM_TIME;
    assert(editor_session_control_panel_deserialize(payload, &restored));
    assert(restored.search_enabled);
    assert(strcmp(restored.search_query, "mass") == 0);
    assert(restored.filters_collapsed);
    assert(restored.target_symbols_enabled);
    assert(restored.target_units_enabled);
    assert(restored.target_editor_enabled);
    assert(restored.search_scope == CONTROL_SEARCH_SCOPE_PROJECT_FILES);
    assert(!restored.match_all_enabled);
    assert(restored.match_methods_enabled);
    assert(!restored.match_types_enabled);
    assert(restored.match_vars_enabled);
    assert(!restored.match_tags_enabled);
    assert(restored.match_order[0] == CONTROL_FILTER_BTN_MATCH_VARS);
    assert(restored.match_order[1] == CONTROL_FILTER_BTN_MATCH_METHODS);
    assert(restored.editor_view_mode == CONTROL_EDITOR_VIEW_MARKERS);
    assert(restored.field_name);
    assert(restored.field_type);
    assert(!restored.field_params);
    assert(restored.field_kind);
    assert(restored.unit_dimension_mask == (CONTROL_UNIT_DIM_MASS | CONTROL_UNIT_DIM_FORCE));
    assert(!restored.live_parse_enabled);
    assert(restored.inline_errors_enabled);
    assert(restored.macros_enabled);

    json_object_put(payload);

    json_object* legacyPayload = json_object_new_object();
    json_object_object_add(legacyPayload, "control_panel", json_object_new_object());
    ControlPanelPersistState legacy = sample_state();
    legacy.target_units_enabled = true;
    legacy.unit_dimension_mask = CONTROL_UNIT_DIM_ENERGY;
    assert(editor_session_control_panel_deserialize(legacyPayload, &legacy));
    assert(legacy.target_units_enabled);
    assert(legacy.unit_dimension_mask == CONTROL_UNIT_DIM_ENERGY);
    json_object_put(legacyPayload);

    printf("editor_session_control_panel_test: ok\n");
    return 0;
}
