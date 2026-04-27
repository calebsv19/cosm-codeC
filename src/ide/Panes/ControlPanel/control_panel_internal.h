#ifndef CONTROL_PANEL_INTERNAL_H
#define CONTROL_PANEL_INTERNAL_H

#include "ide/Panes/ControlPanel/control_panel.h"

#include <stdint.h>

#include "core/Analysis/analysis_symbols_store.h"
#include "ide/UI/Trees/ui_tree_node.h"
#include "ide/UI/panel_text_edit.h"

enum { CONTROL_PANEL_SEARCH_MAX = 256 };
enum { CONTROL_FILTER_BUTTON_MAX = 48 };

enum {
    CONTROL_FIELD_NAME   = 1 << 0,
    CONTROL_FIELD_TYPE   = 1 << 1,
    CONTROL_FIELD_PARAMS = 1 << 2,
    CONTROL_FIELD_KIND   = 1 << 3
};

enum {
    CONTROL_MATCH_MASK_METHODS = 1u << 0,
    CONTROL_MATCH_MASK_TYPES   = 1u << 1,
    CONTROL_MATCH_MASK_VARS    = 1u << 2,
    CONTROL_MATCH_MASK_TAGS    = 1u << 3
};

typedef struct {
    bool valid;
    TreeNodeType type;
    char full_path[1024];
    char label[256];
    int start_line;
    int start_col;
    FisicsSymbolKind kind;
} ControlPanelSelectionSnapshot;

typedef struct {
    UITreeNode* base_symbol_tree;
    UITreeNode* visible_symbol_tree;
    PaneScrollState symbol_scroll;
    bool symbol_scroll_init;
    SDL_Rect symbol_scroll_track;
    SDL_Rect symbol_scroll_thumb;
    bool visible_tree_dirty;
    ControlPanelSelectionSnapshot pending_selection_snapshot;
    bool pending_select_all_restore;
} ControlPanelTreeState;

typedef struct {
    UIPanelTextFieldButtonStripLayout search_strip_layout;
    SDL_Rect filter_header_rect;
    char search_query[CONTROL_PANEL_SEARCH_MAX];
    int search_cursor;
    bool search_focused;
    bool search_enabled;
    bool filters_collapsed;
    int symbol_list_top;
    UIPanelTaggedRect filter_buttons[CONTROL_FILTER_BUTTON_MAX];
    UIPanelTaggedRectList filter_button_hits;
    bool symbol_selection_all_visible;
} ControlPanelUIState;

typedef struct {
    bool live_parse_enabled;
    bool show_inline_errors;
    bool show_auto_param_names;
    bool show_macros;
    bool target_symbols_enabled;
    bool target_editor_enabled;
    ControlSearchScope search_scope;
    bool match_all_enabled;
    unsigned int match_mask;
    ControlFilterButtonId match_button_order[4];
    ControlFilterButtonId selected_match_button;
    ControlEditorViewMode editor_view_mode;
    unsigned int filter_fields;
} ControlPanelFilterState;

typedef struct {
    char* cached_file_path;
    uint64_t cached_stamp;
    uint64_t pending_symbols_stamp;
    bool pending_symbols_update;
    bool cached_show_auto_params;
    bool cached_show_macros;
    size_t cached_project_file_count;
    bool cached_project_file_count_valid;
    const struct DirEntry* cached_project_root;
} ControlPanelCacheState;

typedef struct {
    ControlPanelTreeState tree;
    ControlPanelUIState ui;
    ControlPanelFilterState filters;
    ControlPanelCacheState cache;
} ControlPanelControllerState;

ControlPanelControllerState* control_panel_state(void);

#define baseSymbolTree (control_panel_state()->tree.base_symbol_tree)
#define visibleSymbolTree (control_panel_state()->tree.visible_symbol_tree)
#define symbolScroll (control_panel_state()->tree.symbol_scroll)
#define symbolScrollInit (control_panel_state()->tree.symbol_scroll_init)
#define symbolScrollTrack (control_panel_state()->tree.symbol_scroll_track)
#define symbolScrollThumb (control_panel_state()->tree.symbol_scroll_thumb)
#define visibleTreeDirty (control_panel_state()->tree.visible_tree_dirty)
#define pendingSelectionSnapshot (control_panel_state()->tree.pending_selection_snapshot)
#define pendingSelectAllRestore (control_panel_state()->tree.pending_select_all_restore)

#define searchStripLayout (control_panel_state()->ui.search_strip_layout)
#define filterHeaderRect (control_panel_state()->ui.filter_header_rect)
#define searchQuery (control_panel_state()->ui.search_query)
#define searchCursor (control_panel_state()->ui.search_cursor)
#define searchFocused (control_panel_state()->ui.search_focused)
#define searchEnabled (control_panel_state()->ui.search_enabled)
#define filtersCollapsed (control_panel_state()->ui.filters_collapsed)
#define symbolListTop (control_panel_state()->ui.symbol_list_top)
#define filterButtons (control_panel_state()->ui.filter_buttons)
#define filterButtonHits (control_panel_state()->ui.filter_button_hits)
#define symbolSelectionAllVisible (control_panel_state()->ui.symbol_selection_all_visible)

#define liveParseEnabled (control_panel_state()->filters.live_parse_enabled)
#define showInlineErrors (control_panel_state()->filters.show_inline_errors)
#define showAutoParamNames (control_panel_state()->filters.show_auto_param_names)
#define showMacros (control_panel_state()->filters.show_macros)
#define targetSymbolsEnabled (control_panel_state()->filters.target_symbols_enabled)
#define targetEditorEnabled (control_panel_state()->filters.target_editor_enabled)
#define searchScope (control_panel_state()->filters.search_scope)
#define matchAllEnabled (control_panel_state()->filters.match_all_enabled)
#define matchMask (control_panel_state()->filters.match_mask)
#define matchButtonOrder (control_panel_state()->filters.match_button_order)
#define selectedMatchButton (control_panel_state()->filters.selected_match_button)
#define editorViewMode (control_panel_state()->filters.editor_view_mode)
#define filterFields (control_panel_state()->filters.filter_fields)

#define cachedFilePath (control_panel_state()->cache.cached_file_path)
#define cachedStamp (control_panel_state()->cache.cached_stamp)
#define pendingSymbolsStamp (control_panel_state()->cache.pending_symbols_stamp)
#define pendingSymbolsUpdate (control_panel_state()->cache.pending_symbols_update)
#define cachedShowAutoParams (control_panel_state()->cache.cached_show_auto_params)
#define cachedShowMacros (control_panel_state()->cache.cached_show_macros)
#define cachedProjectFileCount (control_panel_state()->cache.cached_project_file_count)
#define cachedProjectFileCountValid (control_panel_state()->cache.cached_project_file_count_valid)
#define cachedProjectRoot (control_panel_state()->cache.cached_project_root)

void control_panel_mark_visible_tree_dirty(void);
void reset_symbol_scroll_to_top(void);
bool control_panel_filters_are_default(void);
unsigned int control_match_mask_all_bits(void);
void control_reset_match_button_order(void);
uint32_t control_build_symbol_kind_mask(void);
UIPanelTextEditBuffer control_panel_search_buffer(void);
void control_panel_after_search_text_edit(void);

#endif
