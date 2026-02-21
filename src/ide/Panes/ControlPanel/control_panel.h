#ifndef CONTROL_PANEL_H
#define CONTROL_PANEL_H

#include <stdbool.h>
#include <SDL2/SDL.h>

#include "ide/UI/scroll_manager.h"

struct UITreeNode;
struct UIPane;

typedef enum {
    CONTROL_FILTER_BTN_NONE = 0,
    CONTROL_FILTER_BTN_MODE_SYMBOLS,
    CONTROL_FILTER_BTN_MODE_METHODS,
    CONTROL_FILTER_BTN_MODE_TYPES,
    CONTROL_FILTER_BTN_MODE_TAGS,
    CONTROL_FILTER_BTN_SCOPE_ACTIVE,
    CONTROL_FILTER_BTN_SCOPE_OPEN,
    CONTROL_FILTER_BTN_SCOPE_PROJECT,
    CONTROL_FILTER_BTN_FIELD_NAME,
    CONTROL_FILTER_BTN_FIELD_TYPE,
    CONTROL_FILTER_BTN_FIELD_PARAMS,
    CONTROL_FILTER_BTN_FIELD_KIND,
    CONTROL_FILTER_BTN_LIVE_PARSE,
    CONTROL_FILTER_BTN_INLINE_ERRORS,
    CONTROL_FILTER_BTN_MACROS
} ControlFilterButtonId;

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
void control_panel_set_search_clear_button_rect(SDL_Rect rect);
bool control_panel_point_in_search_box(int x, int y);
bool control_panel_point_in_search_clear_button(int x, int y);
bool control_panel_apply_search_insert(const char* text);
bool control_panel_apply_search_backspace(void);
bool control_panel_apply_search_delete(void);
bool control_panel_search_cursor_left(void);
bool control_panel_search_cursor_right(void);
bool control_panel_search_cursor_home(void);
bool control_panel_search_cursor_end(void);
bool control_panel_clear_search_query(void);

void control_panel_begin_filter_button_frame(void);
void control_panel_register_filter_button(ControlFilterButtonId id, SDL_Rect rect);
ControlFilterButtonId control_panel_hit_filter_button(int x, int y);
bool control_panel_is_filter_button_active(ControlFilterButtonId id);
void control_panel_activate_filter_button(ControlFilterButtonId id);
void control_panel_set_filter_header_rect(SDL_Rect rect);
bool control_panel_point_in_filter_header(int x, int y);
bool control_panel_filters_collapsed(void);
void control_panel_toggle_filters_collapsed(void);

#endif
