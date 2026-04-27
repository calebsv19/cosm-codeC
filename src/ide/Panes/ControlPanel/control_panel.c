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
#include "ide/Panes/ControlPanel/control_panel_internal.h"

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

ControlPanelControllerState* control_panel_state(void) {
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

enum { CONTROL_PANEL_TREE_VIEWPORT_TOP_INSET = 30 };

void reset_symbol_scroll_to_top(void);
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

void control_panel_mark_visible_tree_dirty(void) {
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

    // Capture latest base-tree expansion state before rebuilding from analysis output.
    symbol_tree_cache_note_tree(baseSymbolTree);

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
    int controlsH = d.button_h;
    int infoStartY = controlsY + controlsH + d.row_gap;
    ToolPanelHeaderMetrics metrics = {
        .controls_y = controlsY,
        .controls_h = controlsH,
        .info_start_y = infoStartY,
        .info_line_gap = d.info_line_gap,
        .info_line_count = 1,
        .bottom_padding = d.row_gap + 2,
        .min_content_top = controlsY + controlsH + d.row_gap + d.info_line_gap
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
