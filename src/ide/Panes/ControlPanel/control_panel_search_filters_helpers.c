#include "ide/Panes/ControlPanel/control_panel_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ide/Panes/Editor/editor_view.h"
#include "ide/UI/panel_text_edit.h"
#include "ide/UI/text_input_focus.h"

UIPanelTextEditBuffer control_panel_search_buffer(void) {
    UIPanelTextEditBuffer buffer = {
        searchQuery,
        CONTROL_PANEL_SEARCH_MAX,
        &searchCursor
    };
    return buffer;
}

void control_panel_after_search_text_edit(void) {
    control_panel_mark_visible_tree_dirty();
    reset_symbol_scroll_to_top();
    editor_sync_active_file_projection_mode();
}

void reset_symbol_scroll_to_top(void) {
    if (!symbolScrollInit) return;
    symbolScroll.offset_px = 0.0f;
    symbolScroll.target_offset_px = 0.0f;
}

bool control_panel_filters_are_default(void) {
    return targetSymbolsEnabled &&
           targetEditorEnabled &&
           (searchScope == CONTROL_SEARCH_SCOPE_ACTIVE_FILE) &&
           matchAllEnabled &&
           (editorViewMode == CONTROL_EDITOR_VIEW_PROJECTION) &&
           (filterFields == (CONTROL_FIELD_NAME | CONTROL_FIELD_TYPE |
                             CONTROL_FIELD_PARAMS | CONTROL_FIELD_KIND));
}

unsigned int control_match_mask_all_bits(void) {
    return CONTROL_MATCH_MASK_METHODS |
           CONTROL_MATCH_MASK_TYPES |
           CONTROL_MATCH_MASK_VARS |
           CONTROL_MATCH_MASK_TAGS;
}

static bool control_is_reorderable_match_button(ControlFilterButtonId id) {
    return id == CONTROL_FILTER_BTN_MATCH_METHODS ||
           id == CONTROL_FILTER_BTN_MATCH_TYPES ||
           id == CONTROL_FILTER_BTN_MATCH_VARS ||
           id == CONTROL_FILTER_BTN_MATCH_TAGS;
}

void control_reset_match_button_order(void) {
    matchButtonOrder[0] = CONTROL_FILTER_BTN_MATCH_METHODS;
    matchButtonOrder[1] = CONTROL_FILTER_BTN_MATCH_TYPES;
    matchButtonOrder[2] = CONTROL_FILTER_BTN_MATCH_VARS;
    matchButtonOrder[3] = CONTROL_FILTER_BTN_MATCH_TAGS;
}

uint32_t control_build_symbol_kind_mask(void) {
    if (matchAllEnabled) return 0u;
    uint32_t mask = 0u;
    if ((matchMask & CONTROL_MATCH_MASK_METHODS) != 0u) mask |= SYMBOL_KIND_MASK_METHODS;
    if ((matchMask & CONTROL_MATCH_MASK_TYPES) != 0u) mask |= SYMBOL_KIND_MASK_TYPES;
    if ((matchMask & CONTROL_MATCH_MASK_VARS) != 0u) mask |= SYMBOL_KIND_MASK_VARS;
    if ((matchMask & CONTROL_MATCH_MASK_TAGS) != 0u) mask |= SYMBOL_KIND_MASK_TAGS;
    return mask;
}

const char* control_panel_get_search_query(void) {
    return searchQuery;
}

int control_panel_get_search_cursor(void) {
    return searchCursor;
}

bool control_panel_is_search_focused(void) {
    return searchFocused;
}

void control_panel_set_search_focused(bool focused) {
    (void)ui_text_input_focus_set(&searchFocused, focused);
}

void control_panel_set_search_strip_layout(UIPanelTextFieldButtonStripLayout layout) {
    searchStripLayout = layout;
}

UIPanelTextFieldButtonStripLayout control_panel_get_search_strip_layout(void) {
    return searchStripLayout;
}

bool control_panel_is_search_enabled(void) {
    return searchEnabled;
}

void control_panel_set_search_enabled(bool enabled) {
    if (searchEnabled == enabled) return;
    searchEnabled = enabled;
    control_panel_refresh_visible_symbol_tree();
    reset_symbol_scroll_to_top();
    editor_sync_active_file_projection_mode();
}

void control_panel_toggle_search_enabled(void) {
    control_panel_set_search_enabled(!searchEnabled);
}

void control_panel_set_filter_header_rect(SDL_Rect rect) {
    filterHeaderRect = rect;
}

SDL_Rect control_panel_get_filter_header_rect(void) {
    return filterHeaderRect;
}

bool control_panel_filters_collapsed(void) {
    return filtersCollapsed;
}

void control_panel_toggle_filters_collapsed(void) {
    filtersCollapsed = !filtersCollapsed;
}

void control_panel_capture_persist_state(ControlPanelPersistState* outState) {
    if (!outState) return;

    memset(outState, 0, sizeof(*outState));
    outState->search_enabled = searchEnabled;
    snprintf(outState->search_query, sizeof(outState->search_query), "%s", searchQuery);
    outState->filters_collapsed = filtersCollapsed;

    outState->target_symbols_enabled = targetSymbolsEnabled;
    outState->target_editor_enabled = targetEditorEnabled;
    outState->search_scope = searchScope;

    outState->match_all_enabled = matchAllEnabled;
    outState->match_methods_enabled = (matchMask & CONTROL_MATCH_MASK_METHODS) != 0u;
    outState->match_types_enabled = (matchMask & CONTROL_MATCH_MASK_TYPES) != 0u;
    outState->match_vars_enabled = (matchMask & CONTROL_MATCH_MASK_VARS) != 0u;
    outState->match_tags_enabled = (matchMask & CONTROL_MATCH_MASK_TAGS) != 0u;
    for (int i = 0; i < 4; ++i) {
        outState->match_order[i] = matchButtonOrder[i];
    }

    outState->editor_view_mode = editorViewMode;

    outState->field_name = (filterFields & CONTROL_FIELD_NAME) != 0;
    outState->field_type = (filterFields & CONTROL_FIELD_TYPE) != 0;
    outState->field_params = (filterFields & CONTROL_FIELD_PARAMS) != 0;
    outState->field_kind = (filterFields & CONTROL_FIELD_KIND) != 0;

    outState->live_parse_enabled = liveParseEnabled;
    outState->inline_errors_enabled = showInlineErrors;
    outState->macros_enabled = showMacros;
}

void control_panel_apply_persist_state(const ControlPanelPersistState* state) {
    if (!state) return;

    searchEnabled = state->search_enabled;
    snprintf(searchQuery, sizeof(searchQuery), "%s", state->search_query);
    searchCursor = (int)strlen(searchQuery);
    if (searchCursor < 0) searchCursor = 0;
    if (searchCursor >= CONTROL_PANEL_SEARCH_MAX) {
        searchCursor = CONTROL_PANEL_SEARCH_MAX - 1;
    }
    searchFocused = false;

    filtersCollapsed = state->filters_collapsed;
    targetSymbolsEnabled = state->target_symbols_enabled;
    targetEditorEnabled = state->target_editor_enabled;
    searchScope = state->search_scope;
    if (searchScope != CONTROL_SEARCH_SCOPE_ACTIVE_FILE &&
        searchScope != CONTROL_SEARCH_SCOPE_PROJECT_FILES) {
        searchScope = CONTROL_SEARCH_SCOPE_ACTIVE_FILE;
    }

    matchAllEnabled = state->match_all_enabled;
    matchMask = 0u;
    if (state->match_methods_enabled) matchMask |= CONTROL_MATCH_MASK_METHODS;
    if (state->match_types_enabled) matchMask |= CONTROL_MATCH_MASK_TYPES;
    if (state->match_vars_enabled) matchMask |= CONTROL_MATCH_MASK_VARS;
    if (state->match_tags_enabled) matchMask |= CONTROL_MATCH_MASK_TAGS;
    if (matchMask == 0u) {
        matchMask = control_match_mask_all_bits();
    }
    control_reset_match_button_order();
    {
        bool seenMethods = false;
        bool seenTypes = false;
        bool seenVars = false;
        bool seenTags = false;
        ControlFilterButtonId rebuilt[4] = {0};
        int outAt = 0;
        for (int i = 0; i < 4; ++i) {
            ControlFilterButtonId id = state->match_order[i];
            if (id == CONTROL_FILTER_BTN_MATCH_METHODS && !seenMethods) {
                seenMethods = true;
                rebuilt[outAt++] = id;
            } else if (id == CONTROL_FILTER_BTN_MATCH_TYPES && !seenTypes) {
                seenTypes = true;
                rebuilt[outAt++] = id;
            } else if (id == CONTROL_FILTER_BTN_MATCH_VARS && !seenVars) {
                seenVars = true;
                rebuilt[outAt++] = id;
            } else if (id == CONTROL_FILTER_BTN_MATCH_TAGS && !seenTags) {
                seenTags = true;
                rebuilt[outAt++] = id;
            }
        }
        if (!seenMethods) rebuilt[outAt++] = CONTROL_FILTER_BTN_MATCH_METHODS;
        if (!seenTypes) rebuilt[outAt++] = CONTROL_FILTER_BTN_MATCH_TYPES;
        if (!seenVars) rebuilt[outAt++] = CONTROL_FILTER_BTN_MATCH_VARS;
        if (!seenTags) rebuilt[outAt++] = CONTROL_FILTER_BTN_MATCH_TAGS;
        for (int i = 0; i < 4; ++i) {
            matchButtonOrder[i] = rebuilt[i];
        }
    }
    selectedMatchButton = CONTROL_FILTER_BTN_NONE;

    editorViewMode = state->editor_view_mode;
    if (editorViewMode != CONTROL_EDITOR_VIEW_PROJECTION &&
        editorViewMode != CONTROL_EDITOR_VIEW_MARKERS) {
        editorViewMode = CONTROL_EDITOR_VIEW_PROJECTION;
    }

    filterFields = 0u;
    if (state->field_name) filterFields |= CONTROL_FIELD_NAME;
    if (state->field_type) filterFields |= CONTROL_FIELD_TYPE;
    if (state->field_params) filterFields |= CONTROL_FIELD_PARAMS;
    if (state->field_kind) filterFields |= CONTROL_FIELD_KIND;
    if (filterFields == 0u) {
        filterFields = CONTROL_FIELD_NAME;
    }

    liveParseEnabled = state->live_parse_enabled;
    showInlineErrors = state->inline_errors_enabled;
    showMacros = state->macros_enabled;

    control_panel_mark_visible_tree_dirty();
    reset_symbol_scroll_to_top();
    editor_sync_active_file_projection_mode();
}

bool control_panel_apply_search_insert(const char* text) {
    UIPanelTextEditBuffer buffer = control_panel_search_buffer();
    if (!ui_panel_text_edit_insert(&buffer, text)) return false;
    control_panel_after_search_text_edit();
    return true;
}

bool control_panel_apply_search_backspace(void) {
    UIPanelTextEditBuffer buffer = control_panel_search_buffer();
    if (!ui_panel_text_edit_backspace(&buffer)) return false;
    control_panel_after_search_text_edit();
    return true;
}

bool control_panel_apply_search_delete(void) {
    UIPanelTextEditBuffer buffer = control_panel_search_buffer();
    if (!ui_panel_text_edit_delete(&buffer)) return false;
    control_panel_after_search_text_edit();
    return true;
}

bool control_panel_search_cursor_left(void) {
    UIPanelTextEditBuffer buffer = control_panel_search_buffer();
    return ui_panel_text_edit_move_left(&buffer);
}

bool control_panel_search_cursor_right(void) {
    UIPanelTextEditBuffer buffer = control_panel_search_buffer();
    return ui_panel_text_edit_move_right(&buffer);
}

bool control_panel_search_cursor_home(void) {
    UIPanelTextEditBuffer buffer = control_panel_search_buffer();
    return ui_panel_text_edit_move_home(&buffer);
}

bool control_panel_search_cursor_end(void) {
    UIPanelTextEditBuffer buffer = control_panel_search_buffer();
    return ui_panel_text_edit_move_end(&buffer);
}

bool control_panel_clear_search_query(void) {
    UIPanelTextEditBuffer buffer = control_panel_search_buffer();
    if (!ui_panel_text_edit_clear(&buffer)) return false;
    control_panel_after_search_text_edit();
    return true;
}

bool control_panel_handle_search_text_input(const SDL_Event* event) {
    if (!event || event->type != SDL_TEXTINPUT) return false;
    UIPanelTextEditBuffer buffer = control_panel_search_buffer();
    if (!ui_panel_text_edit_handle_text_input(&buffer, event)) return false;
    control_panel_after_search_text_edit();
    return true;
}

bool control_panel_handle_search_edit_key(SDL_Keycode key) {
    UIPanelTextEditBuffer buffer = control_panel_search_buffer();
    if (!ui_panel_text_edit_handle_keydown(&buffer, key)) return false;
    if (key == SDLK_BACKSPACE || key == SDLK_DELETE) {
        control_panel_after_search_text_edit();
    }
    return true;
}

bool control_panel_has_active_search_state(void) {
    if (searchEnabled && searchQuery[0] != '\0') return true;
    return !control_panel_filters_are_default();
}

void control_panel_get_search_filter_options(SymbolFilterOptions* outOptions) {
    if (!outOptions) return;
    outOptions->mode = SYMBOL_FILTER_MODE_SYMBOLS;
    outOptions->kind_mask = control_build_symbol_kind_mask();
    outOptions->scope = (searchScope == CONTROL_SEARCH_SCOPE_PROJECT_FILES)
                            ? SYMBOL_FILTER_SCOPE_PROJECT
                            : SYMBOL_FILTER_SCOPE_ACTIVE;
    outOptions->field_name = (filterFields & CONTROL_FIELD_NAME) != 0;
    outOptions->field_type = (filterFields & CONTROL_FIELD_TYPE) != 0;
    outOptions->field_params = (filterFields & CONTROL_FIELD_PARAMS) != 0;
    outOptions->field_kind = (filterFields & CONTROL_FIELD_KIND) != 0;
}

bool control_panel_target_symbols_enabled(void) {
    return targetSymbolsEnabled;
}

bool control_panel_target_editor_enabled(void) {
    return targetEditorEnabled;
}

ControlSearchScope control_panel_get_search_scope(void) {
    return searchScope;
}

ControlMatchKind control_panel_get_match_kind(void) {
    if (matchAllEnabled) return CONTROL_MATCH_KIND_ALL;
    if (matchMask == CONTROL_MATCH_MASK_METHODS) return CONTROL_MATCH_KIND_METHODS;
    if (matchMask == CONTROL_MATCH_MASK_TYPES) return CONTROL_MATCH_KIND_TYPES;
    if (matchMask == CONTROL_MATCH_MASK_VARS) return CONTROL_MATCH_KIND_VARS;
    if (matchMask == CONTROL_MATCH_MASK_TAGS) return CONTROL_MATCH_KIND_TAGS;
    return CONTROL_MATCH_KIND_ALL;
}

ControlEditorViewMode control_panel_get_editor_view_mode(void) {
    return editorViewMode;
}

void control_panel_set_target_symbols_enabled(bool enabled) {
    targetSymbolsEnabled = enabled;
    control_panel_refresh_visible_symbol_tree();
    reset_symbol_scroll_to_top();
    editor_sync_active_file_projection_mode();
}

void control_panel_set_target_editor_enabled(bool enabled) {
    targetEditorEnabled = enabled;
    editor_sync_active_file_projection_mode();
}

void control_panel_set_search_scope(ControlSearchScope scope) {
    searchScope = scope;
    control_panel_refresh_visible_symbol_tree();
    reset_symbol_scroll_to_top();
    editor_sync_active_file_projection_mode();
}

void control_panel_set_match_kind(ControlMatchKind kind) {
    matchAllEnabled = false;
    switch (kind) {
        case CONTROL_MATCH_KIND_METHODS: matchMask = CONTROL_MATCH_MASK_METHODS; break;
        case CONTROL_MATCH_KIND_TYPES: matchMask = CONTROL_MATCH_MASK_TYPES; break;
        case CONTROL_MATCH_KIND_VARS: matchMask = CONTROL_MATCH_MASK_VARS; break;
        case CONTROL_MATCH_KIND_TAGS: matchMask = CONTROL_MATCH_MASK_TAGS; break;
        case CONTROL_MATCH_KIND_ALL:
        default:
            matchAllEnabled = true;
            matchMask = control_match_mask_all_bits();
            break;
    }
    control_panel_refresh_visible_symbol_tree();
    reset_symbol_scroll_to_top();
    editor_sync_active_file_projection_mode();
}

void control_panel_set_editor_view_mode(ControlEditorViewMode mode) {
    editorViewMode = mode;
    editor_sync_active_file_projection_mode();
}

void control_panel_get_match_button_order(ControlFilterButtonId outOrder[4]) {
    if (!outOrder) return;
    for (int i = 0; i < 4; ++i) {
        outOrder[i] = matchButtonOrder[i];
    }
}

bool control_panel_is_match_button_selected(ControlFilterButtonId id) {
    if (!control_is_reorderable_match_button(id)) return false;
    return selectedMatchButton == id;
}

bool control_panel_select_match_button(ControlFilterButtonId id) {
    if (!control_is_reorderable_match_button(id)) return false;
    selectedMatchButton = id;
    return true;
}

void control_panel_clear_match_button_selection(void) {
    selectedMatchButton = CONTROL_FILTER_BTN_NONE;
}

bool control_panel_move_selected_match_button(int direction, bool jump_to_edge) {
    if (!control_is_reorderable_match_button(selectedMatchButton)) return false;
    if (direction == 0 && !jump_to_edge) return false;

    int current = -1;
    for (int i = 0; i < 4; ++i) {
        if (matchButtonOrder[i] == selectedMatchButton) {
            current = i;
            break;
        }
    }
    if (current < 0) return false;

    int target = current;
    if (jump_to_edge) {
        target = direction < 0 ? 0 : 3;
    } else if (direction < 0) {
        target = current - 1;
    } else if (direction > 0) {
        target = current + 1;
    }

    if (target < 0) target = 0;
    if (target > 3) target = 3;
    if (target == current) return false;

    ControlFilterButtonId id = matchButtonOrder[current];
    if (target > current) {
        for (int i = current; i < target; ++i) {
            matchButtonOrder[i] = matchButtonOrder[i + 1];
        }
    } else {
        for (int i = current; i > target; --i) {
            matchButtonOrder[i] = matchButtonOrder[i - 1];
        }
    }
    matchButtonOrder[target] = id;
    selectedMatchButton = id;
    reset_symbol_scroll_to_top();
    control_panel_refresh_visible_symbol_tree();
    return true;
}
