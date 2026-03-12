#include "control_panel.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/Analysis/analysis_symbols_store.h"
#include "ide/Panes/ControlPanel/symbol_tree_adapter.h"
#include "ide/UI/Trees/tree_renderer.h"
#include "ide/UI/Trees/tree_snapshot.h"
#include "ide/UI/Trees/ui_tree_node.h"
#include "ide/UI/panel_text_edit.h"
#include "ide/UI/text_input_focus.h"
#include "ide/UI/ui_state.h"
#include "ide/Panes/ToolPanels/tool_panel_chrome.h"
#include "ide/Panes/ToolPanels/tool_panel_top_layout.h"
#include "ide/Panes/PaneInfo/pane.h"
#include "ide/Panes/Editor/editor_view.h"
#include "app/GlobalInfo/core_state.h"
#include "app/GlobalInfo/project.h"
#include "core/Clipboard/clipboard.h"

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
    const DirEntry* cached_project_root;
} ControlPanelCacheState;

typedef struct {
    ControlPanelTreeState tree;
    ControlPanelUIState ui;
    ControlPanelFilterState filters;
    ControlPanelCacheState cache;
} ControlPanelControllerState;

static ControlPanelControllerState g_controlPanelBootstrapState;

static void control_panel_init_controller_state(ControlPanelControllerState* state) {
    if (!state) return;

    memset(state, 0, sizeof(*state));
    state->tree.visible_tree_dirty = true;
    state->ui.search_enabled = true;
    state->filters.target_symbols_enabled = true;
    state->filters.target_editor_enabled = true;
    state->filters.search_scope = CONTROL_SEARCH_SCOPE_ACTIVE_FILE;
    state->filters.match_all_enabled = true;
    state->filters.match_mask = CONTROL_MATCH_MASK_METHODS |
                                CONTROL_MATCH_MASK_TYPES |
                                CONTROL_MATCH_MASK_VARS |
                                CONTROL_MATCH_MASK_TAGS;
    state->filters.match_button_order[0] = CONTROL_FILTER_BTN_MATCH_METHODS;
    state->filters.match_button_order[1] = CONTROL_FILTER_BTN_MATCH_TYPES;
    state->filters.match_button_order[2] = CONTROL_FILTER_BTN_MATCH_VARS;
    state->filters.match_button_order[3] = CONTROL_FILTER_BTN_MATCH_TAGS;
    state->filters.selected_match_button = CONTROL_FILTER_BTN_NONE;
    state->filters.editor_view_mode = CONTROL_EDITOR_VIEW_PROJECTION;
    state->filters.filter_fields = CONTROL_FIELD_NAME | CONTROL_FIELD_TYPE |
                                   CONTROL_FIELD_PARAMS | CONTROL_FIELD_KIND;
    state->ui.filter_button_hits.items = state->ui.filter_buttons;
    state->ui.filter_button_hits.count = 0;
    state->ui.filter_button_hits.capacity = CONTROL_FILTER_BUTTON_MAX;
}

static void control_panel_destroy_controller_state(void* ptr) {
    ControlPanelControllerState* state = (ControlPanelControllerState*)ptr;
    if (!state) return;

    if (tree_select_all_visual_active_for(state->tree.visible_symbol_tree) ||
        tree_select_all_visual_active_for(state->tree.base_symbol_tree)) {
        clearTreeSelectAllVisual();
    }
    if (state->tree.visible_symbol_tree &&
        state->tree.visible_symbol_tree != state->tree.base_symbol_tree) {
        freeTreeNodeRecursive(state->tree.visible_symbol_tree);
    }
    if (state->tree.base_symbol_tree) {
        freeTreeNodeRecursive(state->tree.base_symbol_tree);
    }
    free(state->cache.cached_file_path);
    free(state);
}

void control_panel_attach_controller(UIPane* pane) {
    if (!pane || pane->controllerState) return;

    ControlPanelControllerState* state = (ControlPanelControllerState*)malloc(sizeof(*state));
    if (!state) return;

    control_panel_init_controller_state(state);
    pane->controllerState = state;
    pane->destroyControllerState = control_panel_destroy_controller_state;
}

static ControlPanelControllerState* control_panel_state(void) {
    UIState* ui = getUIState();
    UIPane* pane = ui ? ui->controlPanel : NULL;
    if (pane) {
        if (!pane->controllerState) {
            control_panel_attach_controller(pane);
        }
        if (pane->controllerState) {
            return (ControlPanelControllerState*)pane->controllerState;
        }
    }
    if (g_controlPanelBootstrapState.ui.filter_button_hits.items == NULL) {
        control_panel_init_controller_state(&g_controlPanelBootstrapState);
    }
    return &g_controlPanelBootstrapState;
}

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

enum { CONTROL_PANEL_TREE_VIEWPORT_TOP_INSET = 30 };

static void reset_symbol_scroll_to_top(void);
static void control_panel_rebuild_visible_tree_if_needed(void);

static void control_panel_ensure_ui_state_links(void) {
    if (filterButtonHits.items == filterButtons &&
        filterButtonHits.capacity == CONTROL_FILTER_BUTTON_MAX) {
        return;
    }
    filterButtonHits.items = filterButtons;
    filterButtonHits.count = 0;
    filterButtonHits.capacity = CONTROL_FILTER_BUTTON_MAX;
}

static void control_panel_mark_visible_tree_dirty(void) {
    visibleTreeDirty = true;
}

static bool control_panel_tree_contains_node_pointer(const UITreeNode* root,
                                                     const UITreeNode* target) {
    if (!root || !target) return false;
    if (root == target) return true;
    for (int i = 0; i < root->childCount; ++i) {
        if (control_panel_tree_contains_node_pointer(root->children[i], target)) {
            return true;
        }
    }
    return false;
}

static UITreeNode* control_panel_resolve_selected_live_node(void) {
    UITreeNode* selected = getSelectedTreeNode();
    if (!selected) return NULL;

    if (control_panel_tree_contains_node_pointer(visibleSymbolTree, selected)) {
        return selected;
    }
    if (baseSymbolTree != visibleSymbolTree &&
        control_panel_tree_contains_node_pointer(baseSymbolTree, selected)) {
        return selected;
    }

    clearTreeSelectionState();
    return NULL;
}

static void control_panel_capture_selection_snapshot(ControlPanelSelectionSnapshot* snapshot) {
    if (!snapshot) return;
    memset(snapshot, 0, sizeof(*snapshot));

    UITreeNode* selected = control_panel_resolve_selected_live_node();
    if (!selected) return;

    snapshot->valid = true;
    snapshot->type = selected->type;
    if (selected->fullPath) {
        snprintf(snapshot->full_path, sizeof(snapshot->full_path), "%s", selected->fullPath);
    }
    if (selected->label) {
        snprintf(snapshot->label, sizeof(snapshot->label), "%s", selected->label);
    }

    const FisicsSymbol* sym = (const FisicsSymbol*)selected->userData;
    if (sym) {
        snapshot->start_line = sym->start_line;
        snapshot->start_col = sym->start_col;
        snapshot->kind = sym->kind;
    }
}

static bool control_panel_selection_snapshot_matches_node(const ControlPanelSelectionSnapshot* snapshot,
                                                          const UITreeNode* node) {
    if (!snapshot || !snapshot->valid || !node) return false;
    if (node->type != snapshot->type) return false;

    if (snapshot->full_path[0]) {
        if (!node->fullPath || strcmp(node->fullPath, snapshot->full_path) != 0) return false;
    } else if (node->fullPath && node->fullPath[0]) {
        return false;
    }

    if (snapshot->label[0]) {
        if (!node->label || strcmp(node->label, snapshot->label) != 0) return false;
    } else if (node->label && node->label[0]) {
        return false;
    }

    if (snapshot->start_line > 0 || snapshot->start_col > 0) {
        const FisicsSymbol* sym = (const FisicsSymbol*)node->userData;
        if (!sym) return false;
        if (sym->start_line != snapshot->start_line ||
            sym->start_col != snapshot->start_col ||
            sym->kind != snapshot->kind) {
            return false;
        }
    }

    return true;
}

static UITreeNode* control_panel_find_matching_node(UITreeNode* root,
                                                    const ControlPanelSelectionSnapshot* snapshot) {
    if (!root || !snapshot || !snapshot->valid) return NULL;
    if (control_panel_selection_snapshot_matches_node(snapshot, root)) {
        return root;
    }

    for (int i = 0; i < root->childCount; ++i) {
        UITreeNode* found = control_panel_find_matching_node(root->children[i], snapshot);
        if (found) return found;
    }
    return NULL;
}

static bool control_panel_tree_strings_equal(const char* a, const char* b) {
    if (a == b) return true;
    if (!a || !b) return false;
    return strcmp(a, b) == 0;
}

static bool control_panel_tree_nodes_equivalent(const UITreeNode* a, const UITreeNode* b) {
    if (a == b) return true;
    if (!a || !b) return false;
    if (a->type != b->type ||
        a->color != b->color ||
        a->isExpanded != b->isExpanded ||
        a->depth != b->depth ||
        a->childCount != b->childCount) {
        return false;
    }
    if (!control_panel_tree_strings_equal(a->label, b->label) ||
        !control_panel_tree_strings_equal(a->fullPath, b->fullPath)) {
        return false;
    }

    for (int i = 0; i < a->childCount; ++i) {
        if (!control_panel_tree_nodes_equivalent(a->children[i], b->children[i])) {
            return false;
        }
    }
    return true;
}

static UIPanelTextEditBuffer control_panel_search_buffer(void) {
    UIPanelTextEditBuffer buffer = {
        searchQuery,
        CONTROL_PANEL_SEARCH_MAX,
        &searchCursor
    };
    return buffer;
}

static void control_panel_after_search_text_edit(void) {
    control_panel_mark_visible_tree_dirty();
    reset_symbol_scroll_to_top();
    editor_sync_active_file_projection_mode();
}

static bool is_ignored_name(const char* name) {
    if (!name) return false;
    return strcmp(name, ".git") == 0 ||
           strcmp(name, "build") == 0 ||
           strcmp(name, "ide_files") == 0 ||
           strcmp(name, ".DS_Store") == 0;
}

static bool is_allowed_root_dir(const char* name) {
    if (!name) return false;
    return strcmp(name, "src") == 0 || strcmp(name, "include") == 0;
}

static void clear_visible_tree_only(void) {
    clearTreeSelectionState();
    if (visibleSymbolTree && visibleSymbolTree != baseSymbolTree) {
        freeTreeNodeRecursive(visibleSymbolTree);
    }
    visibleSymbolTree = NULL;
}

static void reset_symbol_scroll_to_top(void) {
    if (!symbolScrollInit) return;
    symbolScroll.offset_px = 0.0f;
    symbolScroll.target_offset_px = 0.0f;
}

static bool control_panel_filters_are_default(void) {
    return targetSymbolsEnabled &&
           targetEditorEnabled &&
           (searchScope == CONTROL_SEARCH_SCOPE_ACTIVE_FILE) &&
           matchAllEnabled &&
           (editorViewMode == CONTROL_EDITOR_VIEW_PROJECTION) &&
           (filterFields == (CONTROL_FIELD_NAME | CONTROL_FIELD_TYPE |
                             CONTROL_FIELD_PARAMS | CONTROL_FIELD_KIND));
}

static unsigned int control_match_mask_all_bits(void) {
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

static void control_reset_match_button_order(void) {
    matchButtonOrder[0] = CONTROL_FILTER_BTN_MATCH_METHODS;
    matchButtonOrder[1] = CONTROL_FILTER_BTN_MATCH_TYPES;
    matchButtonOrder[2] = CONTROL_FILTER_BTN_MATCH_VARS;
    matchButtonOrder[3] = CONTROL_FILTER_BTN_MATCH_TAGS;
}

static uint32_t control_build_symbol_kind_mask(void) {
    if (matchAllEnabled) return 0u;
    uint32_t mask = 0u;
    if ((matchMask & CONTROL_MATCH_MASK_METHODS) != 0u) mask |= SYMBOL_KIND_MASK_METHODS;
    if ((matchMask & CONTROL_MATCH_MASK_TYPES) != 0u) mask |= SYMBOL_KIND_MASK_TYPES;
    if ((matchMask & CONTROL_MATCH_MASK_VARS) != 0u) mask |= SYMBOL_KIND_MASK_VARS;
    if ((matchMask & CONTROL_MATCH_MASK_TAGS) != 0u) mask |= SYMBOL_KIND_MASK_TAGS;
    return mask;
}

static UITreeNode* build_empty_search_tree(const char* message) {
    UITreeNode* root = createTreeNode("Symbols", TREE_NODE_SECTION, NODE_COLOR_SECTION, NULL, NULL);
    if (!root) return NULL;
    root->isExpanded = true;
    UITreeNode* line = createTreeNode(message ? message : "No matches", TREE_NODE_FILE, NODE_COLOR_DEFAULT, NULL, NULL);
    if (!line) {
        freeTreeNodeRecursive(root);
        return NULL;
    }
    addChildNode(root, line);
    return root;
}

bool isLiveParseEnabled() {
    return liveParseEnabled;
}

bool isShowInlineErrorsEnabled() {
    return showInlineErrors;
}

bool isShowAutoParamNamesEnabled() {
    return showAutoParamNames;
}

bool isShowMacrosEnabled() {
    return showMacros;
}

void toggleLiveParse() {
    liveParseEnabled = !liveParseEnabled;
}

void toggleShowInlineErrors() {
    showInlineErrors = !showInlineErrors;
}

void toggleShowAutoParamNames() {
    showAutoParamNames = !showAutoParamNames;
}

void toggleShowMacros() {
    showMacros = !showMacros;
}

static void free_cached_path(void) {
    free(cachedFilePath);
    cachedFilePath = NULL;
}

static uint64_t compute_store_stamp(void) {
    return analysis_symbols_store_combined_stamp();
}

void control_panel_note_symbol_store_updated(const char* project_root,
                                             uint64_t symbols_stamp) {
    (void)project_root;
    pendingSymbolsStamp = symbols_stamp;
    pendingSymbolsUpdate = true;
}

static size_t count_project_files(const DirEntry* entry, int depth) {
    if (!entry) return 0;
    if (entry->name && is_ignored_name(entry->name)) return 0;
    if (depth == 1 && entry->name && !is_allowed_root_dir(entry->name)) return 0;
    if (entry->type == ENTRY_FILE) return 1;
    size_t total = 0;
    for (int i = 0; i < entry->childCount; ++i) {
        total += count_project_files(entry->children[i], depth + 1);
    }
    return total;
}

void control_panel_prepare_for_render(struct IDECoreState* core) {
    OpenFile* active_file = NULL;
    if (core && core->activeEditorView) {
        active_file = getActiveOpenFile(core->activeEditorView);
    }
    control_panel_refresh_symbol_tree(projectRoot, active_file ? active_file->filePath : NULL);
    control_panel_rebuild_visible_tree_if_needed();
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

static void control_panel_rebuild_visible_tree_if_needed(void) {
    if (!visibleTreeDirty) return;

    UITreeNode* currentVisible = visibleSymbolTree;
    ControlPanelSelectionSnapshot selectionSnapshot = {0};
    bool hadSelectAllVisual = false;
    if (pendingSelectionSnapshot.valid) {
        selectionSnapshot = pendingSelectionSnapshot;
        hadSelectAllVisual = pendingSelectAllRestore;
    } else {
        hadSelectAllVisual = tree_select_all_visual_active_for(visibleSymbolTree) ||
                             tree_select_all_visual_active_for(baseSymbolTree);
        control_panel_capture_selection_snapshot(&selectionSnapshot);
    }

    UITreeNode* candidateTree = NULL;
    bool candidateIsBase = false;
    if (!targetSymbolsEnabled) {
        candidateTree = build_empty_search_tree("Symbols target disabled");
    } else {
        SymbolFilterMode symbolMode = SYMBOL_FILTER_MODE_SYMBOLS;
        SymbolFilterScope symbolScope = (searchScope == CONTROL_SEARCH_SCOPE_PROJECT_FILES)
                                            ? SYMBOL_FILTER_SCOPE_PROJECT
                                            : SYMBOL_FILTER_SCOPE_ACTIVE;

        SymbolFilterOptions options = {
            .mode = symbolMode,
            .kind_mask = control_build_symbol_kind_mask(),
            .scope = symbolScope,
            .field_name = (filterFields & CONTROL_FIELD_NAME) != 0,
            .field_type = (filterFields & CONTROL_FIELD_TYPE) != 0,
            .field_params = (filterFields & CONTROL_FIELD_PARAMS) != 0,
            .field_kind = (filterFields & CONTROL_FIELD_KIND) != 0
        };

        if (!baseSymbolTree) {
            candidateTree = build_empty_search_tree("No symbols");
        } else {
            const char* effectiveQuery = searchEnabled ? searchQuery : "";
            bool defaultFilters = control_panel_filters_are_default();
            if (!effectiveQuery[0] && defaultFilters) {
                candidateTree = baseSymbolTree;
                candidateIsBase = true;
            } else {
                candidateTree = symbol_tree_clone_filtered(baseSymbolTree, effectiveQuery, &options);
                if (!candidateTree) {
                    candidateTree = build_empty_search_tree("No matches");
                }
            }
        }
    }

    bool keptCurrentVisible = false;
    if (!candidateIsBase &&
        currentVisible &&
        currentVisible != baseSymbolTree &&
        candidateTree &&
        candidateTree != currentVisible &&
        control_panel_tree_nodes_equivalent(currentVisible, candidateTree)) {
        freeTreeNodeRecursive(candidateTree);
        candidateTree = currentVisible;
        keptCurrentVisible = true;
    }

    if (!keptCurrentVisible) {
        if (currentVisible &&
            currentVisible != baseSymbolTree &&
            currentVisible != candidateTree) {
            freeTreeNodeRecursive(currentVisible);
        }
        visibleSymbolTree = candidateTree;
    } else {
        visibleSymbolTree = currentVisible;
    }

    if (keptCurrentVisible) {
        if (hadSelectAllVisual && visibleSymbolTree) {
            setTreeSelectAllVisualRoot(visibleSymbolTree);
        }
    } else {
        UITreeNode* restoredSelection = control_panel_find_matching_node(visibleSymbolTree, &selectionSnapshot);
        if (restoredSelection) {
            setSelectedTreeNode(restoredSelection);
        } else {
            clearTreeSelectionState();
        }
        if (hadSelectAllVisual && visibleSymbolTree) {
            setTreeSelectAllVisualRoot(visibleSymbolTree);
        }
    }

    pendingSelectionSnapshot.valid = false;
    pendingSelectAllRestore = false;
    visibleTreeDirty = false;
}

void control_panel_refresh_visible_symbol_tree(void) {
    control_panel_mark_visible_tree_dirty();
    control_panel_rebuild_visible_tree_if_needed();
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

void control_panel_reset_symbol_tree(void) {
    control_panel_ensure_ui_state_links();
    if (tree_select_all_visual_active_for(visibleSymbolTree) ||
        tree_select_all_visual_active_for(baseSymbolTree)) {
        clearTreeSelectAllVisual();
    }
    clearTreeSelectionState();
    clear_visible_tree_only();
    if (baseSymbolTree) {
        freeTreeNodeRecursive(baseSymbolTree);
        baseSymbolTree = NULL;
    }
    searchQuery[0] = '\0';
    searchCursor = 0;
    searchFocused = false;
    searchEnabled = true;
    symbolListTop = 0;
    ui_panel_tagged_rect_list_reset(&filterButtonHits);
    filtersCollapsed = false;
    searchStripLayout = (UIPanelTextFieldButtonStripLayout){0};
    filterHeaderRect = (SDL_Rect){0};
    control_reset_match_button_order();
    selectedMatchButton = CONTROL_FILTER_BTN_NONE;
    cachedStamp = 0;
    pendingSymbolsStamp = 0;
    pendingSymbolsUpdate = false;
    cachedShowAutoParams = showAutoParamNames;
    cachedShowMacros = showMacros;
    cachedProjectFileCount = 0;
    cachedProjectFileCountValid = false;
    cachedProjectRoot = NULL;
    symbolSelectionAllVisible = false;
    visibleTreeDirty = true;
    pendingSelectionSnapshot.valid = false;
    pendingSelectAllRestore = false;
    free_cached_path();
    editor_sync_active_file_projection_mode();
}

void control_panel_select_all_visible(void) {
    symbolSelectionAllVisible = true;
}

bool control_panel_copy_visible_symbol_tree(void) {
    UITreeNode* tree = control_panel_get_symbol_tree();
    if (!tree) return false;

    char* snapshot = NULL;
    if (symbolSelectionAllVisible) {
        TreeSnapshotOptions opts = {
            .include_root = true,
            .include_indent = true
        };
        snapshot = tree_build_visible_text_snapshot(tree, &opts);
        symbolSelectionAllVisible = false;
    } else {
        UITreeNode* selected = getSelectedTreeNode();
        if (!selected) return false;
        char line[768];
        const char* prefix = "";
        if (selected->type == TREE_NODE_FOLDER || selected->type == TREE_NODE_SECTION) {
            prefix = selected->isExpanded ? "[-] " : "[+] ";
        }
        snprintf(line, sizeof(line), "%s%s", prefix, selected->label ? selected->label : "");
        snapshot = strdup(line);
    }
    if (!snapshot || !snapshot[0]) {
        free(snapshot);
        return false;
    }

    bool ok = clipboard_copy_text(snapshot);
    free(snapshot);
    return ok;
}

bool control_panel_point_in_symbol_tree_content(const UIPane* pane, int x, int y) {
    if (!pane) return false;
    int listTop = control_panel_get_symbol_list_top(pane);
    return (x >= pane->x && x <= (pane->x + pane->w) &&
            y >= listTop && y <= (pane->y + pane->h));
}

void control_panel_refresh_symbol_tree(const DirEntry* projectRoot,
                                       const char* filePath) {
    bool hasSymbolsUpdate = pendingSymbolsUpdate;
    uint64_t stamp = cachedStamp;
    if (hasSymbolsUpdate) {
        stamp = pendingSymbolsStamp;
    } else if (!baseSymbolTree || cachedStamp == 0) {
        // Bootstrap path for initial render/session restore.
        stamp = compute_store_stamp();
        hasSymbolsUpdate = true;
    }
    bool projectRootChanged = (projectRoot != cachedProjectRoot);
    size_t projectFileCount = cachedProjectFileCount;
    if (hasSymbolsUpdate || projectRootChanged || !cachedProjectFileCountValid) {
        projectFileCount = count_project_files(projectRoot, 0);
    }
    bool fileChanged = ((filePath == NULL) != (cachedFilePath == NULL)) ||
                       (filePath && (!cachedFilePath || strcmp(filePath, cachedFilePath) != 0));
    bool analysisChanged = hasSymbolsUpdate ||
                           (stamp != cachedStamp) ||
                           (cachedShowAutoParams != showAutoParamNames) ||
                           (cachedShowMacros != showMacros) ||
                           (projectFileCount != cachedProjectFileCount) ||
                           projectRootChanged;
    bool needsRebuild = fileChanged ||
                        analysisChanged;
    if (!needsRebuild) return;

    pendingSelectAllRestore = tree_select_all_visual_active_for(visibleSymbolTree) ||
                              tree_select_all_visual_active_for(baseSymbolTree);
    control_panel_capture_selection_snapshot(&pendingSelectionSnapshot);

    clearTreeSelectAllVisual();
    UITreeNode* oldBaseTree = baseSymbolTree;
    UITreeNode* rebuiltBaseTree = buildSymbolTreeForWorkspace(projectRoot, filePath, showAutoParamNames, showMacros);
    if (control_panel_tree_nodes_equivalent(oldBaseTree, rebuiltBaseTree)) {
        if (rebuiltBaseTree && rebuiltBaseTree != oldBaseTree) {
            freeTreeNodeRecursive(rebuiltBaseTree);
        }
        rebuiltBaseTree = oldBaseTree;
    }

    if (rebuiltBaseTree != oldBaseTree) {
        clear_visible_tree_only();
        baseSymbolTree = rebuiltBaseTree;
        if (oldBaseTree) {
            freeTreeNodeRecursive(oldBaseTree);
        }
        control_panel_mark_visible_tree_dirty();
    }
    cachedStamp = stamp;
    pendingSymbolsUpdate = false;
    cachedShowAutoParams = showAutoParamNames;
    cachedShowMacros = showMacros;
    cachedProjectFileCount = projectFileCount;
    cachedProjectFileCountValid = true;
    cachedProjectRoot = projectRoot;

    free_cached_path();
    cachedFilePath = filePath ? strdup(filePath) : NULL;

    if (fileChanged && symbolScrollInit) {
        reset_symbol_scroll_to_top();
    }
    if (rebuiltBaseTree == oldBaseTree) {
        pendingSelectionSnapshot.valid = false;
        pendingSelectAllRestore = false;
    }
}

UITreeNode* control_panel_get_symbol_tree(void) {
    control_panel_rebuild_visible_tree_if_needed();
    return visibleSymbolTree;
}

PaneScrollState* control_panel_get_symbol_scroll(void) {
    if (!symbolScrollInit) {
        scroll_state_init(&symbolScroll, NULL);
        symbolScrollInit = true;
    }
    return &symbolScroll;
}

SDL_Rect* control_panel_get_symbol_scroll_track(void) {
    return &symbolScrollTrack;
}

SDL_Rect* control_panel_get_symbol_scroll_thumb(void) {
    return &symbolScrollThumb;
}

int control_panel_get_symbol_list_top(const UIPane* pane) {
    if (symbolListTop > 0) return symbolListTop;
    if (!pane) return 0;
    ToolPanelLayoutDefaults d = tool_panel_layout_defaults();
    int controlsY = pane->y + d.controls_top - 1;
    int controlsH = 22;
    int infoStartY = controlsY + controlsH + d.row_gap;
    ToolPanelHeaderMetrics metrics = {
        .controls_y = controlsY,
        .controls_h = controlsH,
        .info_start_y = infoStartY,
        .info_line_gap = 12,
        .info_line_count = 1,
        .bottom_padding = d.row_gap + 2,
        .min_content_top = controlsY + controlsH + d.row_gap + 12
    };
    return tool_panel_compute_content_top(&metrics) + d.info_line_gap;
}

void control_panel_set_symbol_list_top(int y) {
    symbolListTop = y;
}

int control_panel_get_symbol_tree_origin_y(const UIPane* pane) {
    if (!pane) return 0;
    int listTop = control_panel_get_symbol_list_top(pane);
    int origin = listTop - CONTROL_PANEL_TREE_VIEWPORT_TOP_INSET;
    if (origin < pane->y) origin = pane->y;
    return origin;
}

void control_panel_begin_filter_button_frame(void) {
    control_panel_ensure_ui_state_links();
    ui_panel_tagged_rect_list_reset(&filterButtonHits);
}

UIPanelTaggedRectList* control_panel_get_filter_button_hit_list(void) {
    control_panel_ensure_ui_state_links();
    return &filterButtonHits;
}

ControlFilterButtonId control_panel_hit_filter_button(int x, int y) {
    return (ControlFilterButtonId)ui_panel_tagged_rect_list_hit_test(&filterButtonHits, x, y);
}

bool control_panel_is_filter_button_active(ControlFilterButtonId id) {
    switch (id) {
        case CONTROL_FILTER_BTN_TARGET_SYMBOLS: return targetSymbolsEnabled;
        case CONTROL_FILTER_BTN_TARGET_EDITOR: return targetEditorEnabled;

        case CONTROL_FILTER_BTN_SCOPE_ACTIVE: return searchScope == CONTROL_SEARCH_SCOPE_ACTIVE_FILE;
        case CONTROL_FILTER_BTN_SCOPE_PROJECT:
            return searchScope == CONTROL_SEARCH_SCOPE_PROJECT_FILES;

        case CONTROL_FILTER_BTN_MATCH_ALL: return matchAllEnabled;
        case CONTROL_FILTER_BTN_MATCH_METHODS: return (matchMask & CONTROL_MATCH_MASK_METHODS) != 0u;
        case CONTROL_FILTER_BTN_MATCH_TYPES: return (matchMask & CONTROL_MATCH_MASK_TYPES) != 0u;
        case CONTROL_FILTER_BTN_MATCH_VARS: return (matchMask & CONTROL_MATCH_MASK_VARS) != 0u;
        case CONTROL_FILTER_BTN_MATCH_TAGS: return (matchMask & CONTROL_MATCH_MASK_TAGS) != 0u;

        case CONTROL_FILTER_BTN_EDITOR_VIEW_PROJECTION:
            return editorViewMode == CONTROL_EDITOR_VIEW_PROJECTION;
        case CONTROL_FILTER_BTN_EDITOR_VIEW_MARKERS:
            return editorViewMode == CONTROL_EDITOR_VIEW_MARKERS;

        case CONTROL_FILTER_BTN_LIVE_PARSE: return isLiveParseEnabled();
        case CONTROL_FILTER_BTN_INLINE_ERRORS: return isShowInlineErrorsEnabled();
        case CONTROL_FILTER_BTN_MACROS: return isShowMacrosEnabled();
        default: return false;
    }
}

void control_panel_activate_filter_button(ControlFilterButtonId id) {
    bool changed = false;
    switch (id) {
        case CONTROL_FILTER_BTN_TARGET_SYMBOLS:
            targetSymbolsEnabled = !targetSymbolsEnabled;
            changed = true;
            break;
        case CONTROL_FILTER_BTN_TARGET_EDITOR:
            targetEditorEnabled = !targetEditorEnabled;
            changed = true;
            break;

        case CONTROL_FILTER_BTN_SCOPE_ACTIVE:
            changed = (searchScope != CONTROL_SEARCH_SCOPE_ACTIVE_FILE);
            searchScope = CONTROL_SEARCH_SCOPE_ACTIVE_FILE;
            break;
        case CONTROL_FILTER_BTN_SCOPE_PROJECT:
            changed = (searchScope != CONTROL_SEARCH_SCOPE_PROJECT_FILES);
            searchScope = CONTROL_SEARCH_SCOPE_PROJECT_FILES;
            break;

        case CONTROL_FILTER_BTN_MATCH_ALL: {
            bool next = !matchAllEnabled;
            changed = (matchAllEnabled != next);
            matchAllEnabled = next;
            break;
        }
        case CONTROL_FILTER_BTN_MATCH_METHODS:
            matchMask ^= CONTROL_MATCH_MASK_METHODS;
            changed = true;
            break;
        case CONTROL_FILTER_BTN_MATCH_TYPES:
            matchMask ^= CONTROL_MATCH_MASK_TYPES;
            changed = true;
            break;
        case CONTROL_FILTER_BTN_MATCH_VARS:
            matchMask ^= CONTROL_MATCH_MASK_VARS;
            changed = true;
            break;
        case CONTROL_FILTER_BTN_MATCH_TAGS:
            matchMask ^= CONTROL_MATCH_MASK_TAGS;
            changed = true;
            break;

        case CONTROL_FILTER_BTN_EDITOR_VIEW_PROJECTION:
            changed = (editorViewMode != CONTROL_EDITOR_VIEW_PROJECTION);
            editorViewMode = CONTROL_EDITOR_VIEW_PROJECTION;
            break;
        case CONTROL_FILTER_BTN_EDITOR_VIEW_MARKERS:
            changed = (editorViewMode != CONTROL_EDITOR_VIEW_MARKERS);
            editorViewMode = CONTROL_EDITOR_VIEW_MARKERS;
            break;

        case CONTROL_FILTER_BTN_LIVE_PARSE: toggleLiveParse(); changed = true; break;
        case CONTROL_FILTER_BTN_INLINE_ERRORS: toggleShowInlineErrors(); changed = true; break;
        case CONTROL_FILTER_BTN_MACROS: toggleShowMacros(); changed = true; break;
        default: break;
    }
    if (changed) {
        control_panel_refresh_visible_symbol_tree();
        reset_symbol_scroll_to_top();
        editor_sync_active_file_projection_mode();
    }
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
    if (direction != -1 && direction != 1) return false;

    int selectedIndex = -1;
    for (int i = 0; i < 4; ++i) {
        if (matchButtonOrder[i] == selectedMatchButton) {
            selectedIndex = i;
            break;
        }
    }
    if (selectedIndex < 0) return false;

    int targetIndex = selectedIndex;
    if (jump_to_edge) {
        targetIndex = (direction < 0) ? 0 : 3;
    } else {
        targetIndex = selectedIndex + direction;
    }
    if (targetIndex < 0) targetIndex = 0;
    if (targetIndex > 3) targetIndex = 3;
    if (targetIndex == selectedIndex) return false;

    ControlFilterButtonId moving = matchButtonOrder[selectedIndex];
    if (targetIndex < selectedIndex) {
        for (int i = selectedIndex; i > targetIndex; --i) {
            matchButtonOrder[i] = matchButtonOrder[i - 1];
        }
    } else {
        for (int i = selectedIndex; i < targetIndex; ++i) {
            matchButtonOrder[i] = matchButtonOrder[i + 1];
        }
    }
    matchButtonOrder[targetIndex] = moving;

    editor_sync_active_file_projection_mode();
    return true;
}
