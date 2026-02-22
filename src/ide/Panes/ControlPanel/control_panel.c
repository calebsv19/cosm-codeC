#include "control_panel.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/Analysis/analysis_symbols_store.h"
#include "ide/Panes/ControlPanel/symbol_tree_adapter.h"
#include "ide/UI/Trees/tree_renderer.h"
#include "ide/UI/Trees/ui_tree_node.h"
#include "ide/Panes/PaneInfo/pane.h"
#include "ide/Panes/Editor/editor_view.h"
#include "app/GlobalInfo/project.h"

static bool liveParseEnabled = false;
static bool showInlineErrors = false;
static bool showAutoParamNames = false;
static bool showMacros = false;

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
    ControlFilterButtonId id;
    SDL_Rect rect;
} ControlFilterButtonRect;

static UITreeNode* baseSymbolTree = NULL;
static UITreeNode* visibleSymbolTree = NULL;
static PaneScrollState symbolScroll;
static bool symbolScrollInit = false;
static SDL_Rect symbolScrollTrack = {0};
static SDL_Rect symbolScrollThumb = {0};
static SDL_Rect searchBoxRect = {0};
static SDL_Rect searchPauseButtonRect = {0};
static SDL_Rect searchClearButtonRect = {0};
static SDL_Rect filterHeaderRect = {0};
static char searchQuery[CONTROL_PANEL_SEARCH_MAX] = {0};
static int searchCursor = 0;
static bool searchFocused = false;
static bool searchEnabled = true;
static bool filtersCollapsed = false;
static int symbolListTop = 0;
static ControlFilterButtonRect filterButtons[CONTROL_FILTER_BUTTON_MAX];
static int filterButtonCount = 0;
static bool targetSymbolsEnabled = true;
static bool targetEditorEnabled = true;
static ControlSearchScope searchScope = CONTROL_SEARCH_SCOPE_ACTIVE_FILE;
static bool matchAllEnabled = true;
static unsigned int matchMask = CONTROL_MATCH_MASK_METHODS |
                                CONTROL_MATCH_MASK_TYPES |
                                CONTROL_MATCH_MASK_VARS |
                                CONTROL_MATCH_MASK_TAGS;
static ControlFilterButtonId matchButtonOrder[4] = {
    CONTROL_FILTER_BTN_MATCH_METHODS,
    CONTROL_FILTER_BTN_MATCH_TYPES,
    CONTROL_FILTER_BTN_MATCH_VARS,
    CONTROL_FILTER_BTN_MATCH_TAGS
};
static ControlFilterButtonId selectedMatchButton = CONTROL_FILTER_BTN_NONE;
static ControlEditorViewMode editorViewMode = CONTROL_EDITOR_VIEW_PROJECTION;
static unsigned int filterFields = CONTROL_FIELD_NAME | CONTROL_FIELD_TYPE | CONTROL_FIELD_PARAMS | CONTROL_FIELD_KIND;
static char* cachedFilePath = NULL;
static uint64_t cachedStamp = 0;
static bool cachedShowAutoParams = false;
static bool cachedShowMacros = false;
static size_t cachedProjectFileCount = 0;
static const DirEntry* cachedProjectRoot = NULL;

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
    size_t count = analysis_symbols_store_file_count();
    uint64_t stamp = (uint64_t)count;
    for (size_t i = 0; i < count; ++i) {
        const AnalysisFileSymbols* entry = analysis_symbols_store_file_at(i);
        if (entry) {
            stamp ^= entry->stamp;
        }
    }
    return stamp;
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
    searchFocused = focused;
}

void control_panel_set_search_box_rect(SDL_Rect rect) {
    searchBoxRect = rect;
}

void control_panel_set_search_pause_button_rect(SDL_Rect rect) {
    searchPauseButtonRect = rect;
}

void control_panel_set_search_clear_button_rect(SDL_Rect rect) {
    searchClearButtonRect = rect;
}

bool control_panel_point_in_search_box(int x, int y) {
    return (x >= searchBoxRect.x &&
            x <= (searchBoxRect.x + searchBoxRect.w) &&
            y >= searchBoxRect.y &&
            y <= (searchBoxRect.y + searchBoxRect.h));
}

bool control_panel_point_in_search_pause_button(int x, int y) {
    return (x >= searchPauseButtonRect.x &&
            x <= (searchPauseButtonRect.x + searchPauseButtonRect.w) &&
            y >= searchPauseButtonRect.y &&
            y <= (searchPauseButtonRect.y + searchPauseButtonRect.h));
}

bool control_panel_point_in_search_clear_button(int x, int y) {
    return (x >= searchClearButtonRect.x &&
            x <= (searchClearButtonRect.x + searchClearButtonRect.w) &&
            y >= searchClearButtonRect.y &&
            y <= (searchClearButtonRect.y + searchClearButtonRect.h));
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

bool control_panel_point_in_filter_header(int x, int y) {
    return (x >= filterHeaderRect.x &&
            x <= (filterHeaderRect.x + filterHeaderRect.w) &&
            y >= filterHeaderRect.y &&
            y <= (filterHeaderRect.y + filterHeaderRect.h));
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

    control_panel_refresh_visible_symbol_tree();
    reset_symbol_scroll_to_top();
    editor_sync_active_file_projection_mode();
}

void control_panel_refresh_visible_symbol_tree(void) {
    if (!targetSymbolsEnabled) {
        clear_visible_tree_only();
        visibleSymbolTree = build_empty_search_tree("Symbols target disabled");
        return;
    }

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

    clear_visible_tree_only();

    if (!baseSymbolTree) {
        visibleSymbolTree = build_empty_search_tree("No symbols");
        return;
    }

    const char* effectiveQuery = searchEnabled ? searchQuery : "";
    bool defaultFilters = control_panel_filters_are_default();
    if (!effectiveQuery[0] && defaultFilters) {
        visibleSymbolTree = baseSymbolTree;
        return;
    }

    visibleSymbolTree = symbol_tree_clone_filtered(baseSymbolTree, effectiveQuery, &options);
    if (!visibleSymbolTree) {
        visibleSymbolTree = build_empty_search_tree("No matches");
    }
}

static bool apply_query_text(char* dst, int cap, int* cursor, const char* text) {
    if (!dst || cap <= 1 || !cursor || !text) return false;
    size_t curLen = strlen(dst);
    int cur = *cursor;
    if (cur < 0) cur = 0;
    if ((size_t)cur > curLen) cur = (int)curLen;

    size_t insertLen = strlen(text);
    if (insertLen == 0) return false;
    size_t room = (size_t)(cap - 1) - curLen;
    if (room == 0) return false;
    if (insertLen > room) insertLen = room;

    memmove(dst + cur + insertLen, dst + cur, curLen - (size_t)cur + 1);
    memcpy(dst + cur, text, insertLen);
    *cursor = cur + (int)insertLen;
    return true;
}

bool control_panel_apply_search_insert(const char* text) {
    if (!text || !*text) return false;
    if (!apply_query_text(searchQuery, CONTROL_PANEL_SEARCH_MAX, &searchCursor, text)) {
        return false;
    }
    control_panel_refresh_visible_symbol_tree();
    reset_symbol_scroll_to_top();
    editor_sync_active_file_projection_mode();
    return true;
}

bool control_panel_apply_search_backspace(void) {
    size_t len = strlen(searchQuery);
    if (searchCursor <= 0 || len == 0) return false;
    int removeAt = searchCursor - 1;
    if ((size_t)removeAt >= len) return false;
    memmove(searchQuery + removeAt, searchQuery + removeAt + 1, len - (size_t)removeAt);
    searchCursor = removeAt;
    control_panel_refresh_visible_symbol_tree();
    reset_symbol_scroll_to_top();
    editor_sync_active_file_projection_mode();
    return true;
}

bool control_panel_apply_search_delete(void) {
    size_t len = strlen(searchQuery);
    if (len == 0 || searchCursor < 0 || (size_t)searchCursor >= len) return false;
    memmove(searchQuery + searchCursor, searchQuery + searchCursor + 1, len - (size_t)searchCursor);
    control_panel_refresh_visible_symbol_tree();
    reset_symbol_scroll_to_top();
    editor_sync_active_file_projection_mode();
    return true;
}

bool control_panel_search_cursor_left(void) {
    if (searchCursor <= 0) return false;
    searchCursor--;
    return true;
}

bool control_panel_search_cursor_right(void) {
    size_t len = strlen(searchQuery);
    if ((size_t)searchCursor >= len) return false;
    searchCursor++;
    return true;
}

bool control_panel_search_cursor_home(void) {
    if (searchCursor == 0) return false;
    searchCursor = 0;
    return true;
}

bool control_panel_search_cursor_end(void) {
    int end = (int)strlen(searchQuery);
    if (searchCursor == end) return false;
    searchCursor = end;
    return true;
}

bool control_panel_clear_search_query(void) {
    if (!searchQuery[0] && searchCursor == 0) return false;
    searchQuery[0] = '\0';
    searchCursor = 0;
    control_panel_refresh_visible_symbol_tree();
    reset_symbol_scroll_to_top();
    editor_sync_active_file_projection_mode();
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
    filterButtonCount = 0;
    filtersCollapsed = false;
    searchBoxRect = (SDL_Rect){0};
    searchPauseButtonRect = (SDL_Rect){0};
    searchClearButtonRect = (SDL_Rect){0};
    filterHeaderRect = (SDL_Rect){0};
    control_reset_match_button_order();
    selectedMatchButton = CONTROL_FILTER_BTN_NONE;
    cachedStamp = 0;
    cachedShowAutoParams = showAutoParamNames;
    cachedShowMacros = showMacros;
    cachedProjectFileCount = 0;
    cachedProjectRoot = NULL;
    free_cached_path();
    editor_sync_active_file_projection_mode();
}

void control_panel_refresh_symbol_tree(const DirEntry* projectRoot,
                                       const char* filePath) {
    uint64_t stamp = compute_store_stamp();
    size_t projectFileCount = count_project_files(projectRoot, 0);
    bool fileChanged = ((filePath == NULL) != (cachedFilePath == NULL)) ||
                       (filePath && (!cachedFilePath || strcmp(filePath, cachedFilePath) != 0));
    bool needsRebuild = fileChanged ||
                        (stamp != cachedStamp) ||
                        (cachedShowAutoParams != showAutoParamNames) ||
                        (cachedShowMacros != showMacros) ||
                        (projectFileCount != cachedProjectFileCount) ||
                        (projectRoot != cachedProjectRoot);
    if (!needsRebuild) return;

    clearTreeSelectionState();
    clear_visible_tree_only();
    if (baseSymbolTree) {
        freeTreeNodeRecursive(baseSymbolTree);
        baseSymbolTree = NULL;
    }

    baseSymbolTree = buildSymbolTreeForWorkspace(projectRoot, filePath, showAutoParamNames, showMacros);
    control_panel_refresh_visible_symbol_tree();
    cachedStamp = stamp;
    cachedShowAutoParams = showAutoParamNames;
    cachedShowMacros = showMacros;
    cachedProjectFileCount = projectFileCount;
    cachedProjectRoot = projectRoot;

    free_cached_path();
    cachedFilePath = filePath ? strdup(filePath) : NULL;

    if (fileChanged && symbolScrollInit) {
        reset_symbol_scroll_to_top();
    }
}

UITreeNode* control_panel_get_symbol_tree(void) {
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
    const int padding = 8;
    int y = pane->y + padding;
    y += 24; // title
    y += 14; // search label
    y += 30; // search box
    y += 12; // filters header
    y += 14; // spacing
    return y;
}

void control_panel_set_symbol_list_top(int y) {
    symbolListTop = y;
}

int control_panel_get_symbol_tree_origin_y(const UIPane* pane) {
    if (!pane) return 0;
    int listTop = control_panel_get_symbol_list_top(pane);
    int origin = listTop - 18; // tree renderer adds its own +30 content offset
    if (origin < pane->y) origin = pane->y;
    return origin;
}

void control_panel_begin_filter_button_frame(void) {
    filterButtonCount = 0;
}

void control_panel_register_filter_button(ControlFilterButtonId id, SDL_Rect rect) {
    if (id == CONTROL_FILTER_BTN_NONE) return;
    if (filterButtonCount >= CONTROL_FILTER_BUTTON_MAX) return;
    filterButtons[filterButtonCount].id = id;
    filterButtons[filterButtonCount].rect = rect;
    filterButtonCount++;
}

ControlFilterButtonId control_panel_hit_filter_button(int x, int y) {
    for (int i = 0; i < filterButtonCount; ++i) {
        SDL_Rect r = filterButtons[i].rect;
        if (x >= r.x && x <= r.x + r.w &&
            y >= r.y && y <= r.y + r.h) {
            return filterButtons[i].id;
        }
    }
    return CONTROL_FILTER_BTN_NONE;
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
