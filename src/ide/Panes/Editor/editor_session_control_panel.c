#include "ide/Panes/Editor/editor_session_control_panel.h"

#include <stdio.h>

static void load_bool_field(json_object* obj, const char* key, bool* out) {
    if (!obj || !key || !out) return;
    json_object* jv = NULL;
    if (!json_object_object_get_ex(obj, key, &jv)) return;
    *out = json_object_get_boolean(jv);
}

static void load_int_field(json_object* obj, const char* key, int* out) {
    if (!obj || !key || !out) return;
    json_object* jv = NULL;
    if (!json_object_object_get_ex(obj, key, &jv)) return;
    *out = json_object_get_int(jv);
}

json_object* editor_session_control_panel_serialize(const ControlPanelPersistState* state) {
    if (!state) return NULL;

    json_object* obj = json_object_new_object();
    json_object_object_add(obj, "search_enabled", json_object_new_boolean(state->search_enabled));
    json_object_object_add(obj, "search_query", json_object_new_string(state->search_query));
    json_object_object_add(obj, "filters_collapsed", json_object_new_boolean(state->filters_collapsed));

    json_object_object_add(obj, "target_symbols_enabled", json_object_new_boolean(state->target_symbols_enabled));
    json_object_object_add(obj, "target_units_enabled", json_object_new_boolean(state->target_units_enabled));
    json_object_object_add(obj, "target_editor_enabled", json_object_new_boolean(state->target_editor_enabled));
    json_object_object_add(obj, "search_scope", json_object_new_int((int)state->search_scope));

    json_object_object_add(obj, "match_all_enabled", json_object_new_boolean(state->match_all_enabled));
    json_object_object_add(obj, "match_methods_enabled", json_object_new_boolean(state->match_methods_enabled));
    json_object_object_add(obj, "match_types_enabled", json_object_new_boolean(state->match_types_enabled));
    json_object_object_add(obj, "match_vars_enabled", json_object_new_boolean(state->match_vars_enabled));
    json_object_object_add(obj, "match_tags_enabled", json_object_new_boolean(state->match_tags_enabled));
    json_object* matchOrder = json_object_new_array();
    for (int i = 0; i < 4; ++i) {
        json_object_array_add(matchOrder, json_object_new_int((int)state->match_order[i]));
    }
    json_object_object_add(obj, "match_order", matchOrder);

    json_object_object_add(obj, "editor_view_mode", json_object_new_int((int)state->editor_view_mode));

    json_object_object_add(obj, "field_name", json_object_new_boolean(state->field_name));
    json_object_object_add(obj, "field_type", json_object_new_boolean(state->field_type));
    json_object_object_add(obj, "field_params", json_object_new_boolean(state->field_params));
    json_object_object_add(obj, "field_kind", json_object_new_boolean(state->field_kind));
    json_object_object_add(obj, "unit_dimension_mask", json_object_new_int((int)state->unit_dimension_mask));

    json_object_object_add(obj, "live_parse_enabled", json_object_new_boolean(state->live_parse_enabled));
    json_object_object_add(obj, "inline_errors_enabled", json_object_new_boolean(state->inline_errors_enabled));
    json_object_object_add(obj, "macros_enabled", json_object_new_boolean(state->macros_enabled));
    return obj;
}

bool editor_session_control_panel_deserialize(json_object* payload,
                                              ControlPanelPersistState* inout_state) {
    if (!payload || !json_object_is_type(payload, json_type_object) || !inout_state) {
        return false;
    }
    json_object* jcontrol = NULL;
    if (!json_object_object_get_ex(payload, "control_panel", &jcontrol) ||
        !json_object_is_type(jcontrol, json_type_object)) {
        return false;
    }

    load_bool_field(jcontrol, "search_enabled", &inout_state->search_enabled);
    json_object* jquery = NULL;
    if (json_object_object_get_ex(jcontrol, "search_query", &jquery)) {
        const char* q = json_object_get_string(jquery);
        snprintf(inout_state->search_query, sizeof(inout_state->search_query), "%s", q ? q : "");
    }
    load_bool_field(jcontrol, "filters_collapsed", &inout_state->filters_collapsed);

    load_bool_field(jcontrol, "target_symbols_enabled", &inout_state->target_symbols_enabled);
    load_bool_field(jcontrol, "target_units_enabled", &inout_state->target_units_enabled);
    load_bool_field(jcontrol, "target_editor_enabled", &inout_state->target_editor_enabled);
    {
        int scope = (int)inout_state->search_scope;
        load_int_field(jcontrol, "search_scope", &scope);
        inout_state->search_scope = (ControlSearchScope)scope;
    }

    load_bool_field(jcontrol, "match_all_enabled", &inout_state->match_all_enabled);
    load_bool_field(jcontrol, "match_methods_enabled", &inout_state->match_methods_enabled);
    load_bool_field(jcontrol, "match_types_enabled", &inout_state->match_types_enabled);
    load_bool_field(jcontrol, "match_vars_enabled", &inout_state->match_vars_enabled);
    load_bool_field(jcontrol, "match_tags_enabled", &inout_state->match_tags_enabled);
    json_object* jmatchOrder = NULL;
    if (json_object_object_get_ex(jcontrol, "match_order", &jmatchOrder) &&
        json_object_is_type(jmatchOrder, json_type_array)) {
        int orderCount = (int)json_object_array_length(jmatchOrder);
        if (orderCount > 4) orderCount = 4;
        for (int i = 0; i < orderCount; ++i) {
            json_object* jitem = json_object_array_get_idx(jmatchOrder, (size_t)i);
            if (!jitem) continue;
            inout_state->match_order[i] = (ControlFilterButtonId)json_object_get_int(jitem);
        }
    }

    {
        int editorViewMode = (int)inout_state->editor_view_mode;
        load_int_field(jcontrol, "editor_view_mode", &editorViewMode);
        inout_state->editor_view_mode = (ControlEditorViewMode)editorViewMode;
    }

    load_bool_field(jcontrol, "field_name", &inout_state->field_name);
    load_bool_field(jcontrol, "field_type", &inout_state->field_type);
    load_bool_field(jcontrol, "field_params", &inout_state->field_params);
    load_bool_field(jcontrol, "field_kind", &inout_state->field_kind);
    {
        int unitDimensionMask = (int)inout_state->unit_dimension_mask;
        load_int_field(jcontrol, "unit_dimension_mask", &unitDimensionMask);
        inout_state->unit_dimension_mask = (unsigned int)unitDimensionMask;
    }

    load_bool_field(jcontrol, "live_parse_enabled", &inout_state->live_parse_enabled);
    load_bool_field(jcontrol, "inline_errors_enabled", &inout_state->inline_errors_enabled);
    load_bool_field(jcontrol, "macros_enabled", &inout_state->macros_enabled);
    return true;
}
