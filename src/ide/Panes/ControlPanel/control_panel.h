#ifndef CONTROL_PANEL_H
#define CONTROL_PANEL_H

#include <stdbool.h>
#include <SDL2/SDL.h>

#include "ide/Panes/ControlPanel/symbol_tree_adapter.h"
#include "ide/UI/scroll_manager.h"

struct UITreeNode;
struct UIPane;

typedef enum {
    CONTROL_FILTER_BTN_NONE = 0,
    CONTROL_FILTER_BTN_TARGET_SYMBOLS,
    CONTROL_FILTER_BTN_TARGET_EDITOR,
    CONTROL_FILTER_BTN_SCOPE_ACTIVE,
    CONTROL_FILTER_BTN_SCOPE_PROJECT,
    CONTROL_FILTER_BTN_MATCH_ALL,
    CONTROL_FILTER_BTN_MATCH_METHODS,
    CONTROL_FILTER_BTN_MATCH_TYPES,
    CONTROL_FILTER_BTN_MATCH_VARS,
    CONTROL_FILTER_BTN_MATCH_TAGS,
    CONTROL_FILTER_BTN_EDITOR_VIEW_PROJECTION,
    CONTROL_FILTER_BTN_EDITOR_VIEW_MARKERS,
    CONTROL_FILTER_BTN_LIVE_PARSE,
    CONTROL_FILTER_BTN_INLINE_ERRORS,
    CONTROL_FILTER_BTN_MACROS
} ControlFilterButtonId;

typedef enum {
    CONTROL_SEARCH_SCOPE_ACTIVE_FILE = 0,
    CONTROL_SEARCH_SCOPE_PROJECT_FILES
} ControlSearchScope;

typedef enum {
    CONTROL_MATCH_KIND_ALL = 0,
    CONTROL_MATCH_KIND_METHODS,
    CONTROL_MATCH_KIND_TYPES,
    CONTROL_MATCH_KIND_VARS,
    CONTROL_MATCH_KIND_TAGS
} ControlMatchKind;

typedef enum {
    CONTROL_EDITOR_VIEW_PROJECTION = 0,
    CONTROL_EDITOR_VIEW_MARKERS
} ControlEditorViewMode;

enum { CONTROL_PANEL_PERSIST_QUERY_MAX = 256 };

typedef struct {
    bool search_enabled;
    char search_query[CONTROL_PANEL_PERSIST_QUERY_MAX];
    bool filters_collapsed;

    bool target_symbols_enabled;
    bool target_editor_enabled;
    ControlSearchScope search_scope;

    bool match_all_enabled;
    bool match_methods_enabled;
    bool match_types_enabled;
    bool match_vars_enabled;
    bool match_tags_enabled;
    ControlFilterButtonId match_order[4];

    ControlEditorViewMode editor_view_mode;

    bool field_name;
    bool field_type;
    bool field_params;
    bool field_kind;

    bool live_parse_enabled;
    bool inline_errors_enabled;
    bool macros_enabled;
} ControlPanelPersistState;

bool isLiveParseEnabled();
bool isShowInlineErrorsEnabled();
bool isShowAutoParamNamesEnabled();
bool isShowMacrosEnabled();

void toggleLiveParse();
void toggleShowInlineErrors();
void toggleShowAutoParamNames();
void toggleShowMacros();

struct DirEntry;
void control_panel_refresh_symbol_tree(const struct DirEntry* projectRoot,
                                       const char* filePath);
void control_panel_refresh_visible_symbol_tree(void);
struct UITreeNode* control_panel_get_symbol_tree(void);
PaneScrollState* control_panel_get_symbol_scroll(void);
SDL_Rect* control_panel_get_symbol_scroll_track(void);
SDL_Rect* control_panel_get_symbol_scroll_thumb(void);
int control_panel_get_symbol_list_top(const struct UIPane* pane);
int control_panel_get_symbol_tree_origin_y(const struct UIPane* pane);
void control_panel_set_symbol_list_top(int y);
void control_panel_reset_symbol_tree(void);

const char* control_panel_get_search_query(void);
int control_panel_get_search_cursor(void);
bool control_panel_is_search_focused(void);
void control_panel_set_search_focused(bool focused);
void control_panel_set_search_box_rect(SDL_Rect rect);
void control_panel_set_search_pause_button_rect(SDL_Rect rect);
void control_panel_set_search_clear_button_rect(SDL_Rect rect);
bool control_panel_point_in_search_box(int x, int y);
bool control_panel_point_in_search_pause_button(int x, int y);
bool control_panel_point_in_search_clear_button(int x, int y);
bool control_panel_is_search_enabled(void);
void control_panel_set_search_enabled(bool enabled);
void control_panel_toggle_search_enabled(void);
bool control_panel_apply_search_insert(const char* text);
bool control_panel_apply_search_backspace(void);
bool control_panel_apply_search_delete(void);
bool control_panel_search_cursor_left(void);
bool control_panel_search_cursor_right(void);
bool control_panel_search_cursor_home(void);
bool control_panel_search_cursor_end(void);
bool control_panel_clear_search_query(void);
bool control_panel_has_active_search_state(void);
void control_panel_get_search_filter_options(SymbolFilterOptions* outOptions);
bool control_panel_target_symbols_enabled(void);
bool control_panel_target_editor_enabled(void);
ControlSearchScope control_panel_get_search_scope(void);
ControlMatchKind control_panel_get_match_kind(void);
ControlEditorViewMode control_panel_get_editor_view_mode(void);
void control_panel_set_target_symbols_enabled(bool enabled);
void control_panel_set_target_editor_enabled(bool enabled);
void control_panel_set_search_scope(ControlSearchScope scope);
void control_panel_set_match_kind(ControlMatchKind kind);
void control_panel_set_editor_view_mode(ControlEditorViewMode mode);
void control_panel_get_match_button_order(ControlFilterButtonId outOrder[4]);
bool control_panel_is_match_button_selected(ControlFilterButtonId id);
bool control_panel_select_match_button(ControlFilterButtonId id);
void control_panel_clear_match_button_selection(void);
bool control_panel_move_selected_match_button(int direction, bool jump_to_edge);

void control_panel_begin_filter_button_frame(void);
void control_panel_register_filter_button(ControlFilterButtonId id, SDL_Rect rect);
ControlFilterButtonId control_panel_hit_filter_button(int x, int y);
bool control_panel_is_filter_button_active(ControlFilterButtonId id);
void control_panel_activate_filter_button(ControlFilterButtonId id);
void control_panel_set_filter_header_rect(SDL_Rect rect);
bool control_panel_point_in_filter_header(int x, int y);
bool control_panel_filters_collapsed(void);
void control_panel_toggle_filters_collapsed(void);
void control_panel_capture_persist_state(ControlPanelPersistState* outState);
void control_panel_apply_persist_state(const ControlPanelPersistState* state);

#endif
