#include "control_panel.h"

#include <stdlib.h>
#include <string.h>

#include "core/Analysis/analysis_symbols_store.h"
#include "ide/Panes/ControlPanel/symbol_tree_adapter.h"
#include "ide/UI/Trees/tree_renderer.h"
#include "ide/UI/Trees/ui_tree_node.h"
#include "ide/Panes/PaneInfo/pane.h"
#include "app/GlobalInfo/project.h"

static bool liveParseEnabled = false;
static bool showInlineErrors = false;
static bool showAutoParamNames = false;
static bool showMacros = false;

enum { CONTROL_PANEL_SEARCH_MAX = 256 };
enum { CONTROL_FILTER_BUTTON_MAX = 48 };

typedef enum {
    CONTROL_FILTER_MODE_SYMBOLS = 0,
    CONTROL_FILTER_MODE_METHODS,
    CONTROL_FILTER_MODE_TYPES,
    CONTROL_FILTER_MODE_TAGS
} ControlFilterMode;

typedef enum {
    CONTROL_FILTER_SCOPE_ACTIVE = 0,
    CONTROL_FILTER_SCOPE_OPEN,
    CONTROL_FILTER_SCOPE_PROJECT
} ControlFilterScope;

enum {
    CONTROL_FIELD_NAME   = 1 << 0,
    CONTROL_FIELD_TYPE   = 1 << 1,
    CONTROL_FIELD_PARAMS = 1 << 2,
    CONTROL_FIELD_KIND   = 1 << 3
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
static SDL_Rect searchClearButtonRect = {0};
static SDL_Rect filterHeaderRect = {0};
static char searchQuery[CONTROL_PANEL_SEARCH_MAX] = {0};
static int searchCursor = 0;
static bool searchFocused = false;
static bool filtersCollapsed = false;
static int symbolListTop = 0;
static ControlFilterButtonRect filterButtons[CONTROL_FILTER_BUTTON_MAX];
static int filterButtonCount = 0;
static ControlFilterMode filterMode = CONTROL_FILTER_MODE_SYMBOLS;
static ControlFilterScope filterScope = CONTROL_FILTER_SCOPE_OPEN;
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

void control_panel_set_search_clear_button_rect(SDL_Rect rect) {
    searchClearButtonRect = rect;
}

bool control_panel_point_in_search_box(int x, int y) {
    return (x >= searchBoxRect.x &&
            x <= (searchBoxRect.x + searchBoxRect.w) &&
            y >= searchBoxRect.y &&
            y <= (searchBoxRect.y + searchBoxRect.h));
}

bool control_panel_point_in_search_clear_button(int x, int y) {
    return (x >= searchClearButtonRect.x &&
            x <= (searchClearButtonRect.x + searchClearButtonRect.w) &&
            y >= searchClearButtonRect.y &&
            y <= (searchClearButtonRect.y + searchClearButtonRect.h));
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

void control_panel_refresh_visible_symbol_tree(void) {
    SymbolFilterOptions options = {
        .mode = (SymbolFilterMode)filterMode,
        .scope = (SymbolFilterScope)filterScope,
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

    bool defaultFilters = (filterMode == CONTROL_FILTER_MODE_SYMBOLS) &&
                          (filterScope == CONTROL_FILTER_SCOPE_OPEN) &&
                          (filterFields == (CONTROL_FIELD_NAME | CONTROL_FIELD_TYPE |
                                            CONTROL_FIELD_PARAMS | CONTROL_FIELD_KIND));
    if (!searchQuery[0] && defaultFilters) {
        visibleSymbolTree = baseSymbolTree;
        return;
    }

    visibleSymbolTree = symbol_tree_clone_filtered(baseSymbolTree, searchQuery, &options);
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
    return true;
}

bool control_panel_apply_search_delete(void) {
    size_t len = strlen(searchQuery);
    if (len == 0 || searchCursor < 0 || (size_t)searchCursor >= len) return false;
    memmove(searchQuery + searchCursor, searchQuery + searchCursor + 1, len - (size_t)searchCursor);
    control_panel_refresh_visible_symbol_tree();
    reset_symbol_scroll_to_top();
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
    return true;
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
    symbolListTop = 0;
    filterButtonCount = 0;
    filtersCollapsed = false;
    searchBoxRect = (SDL_Rect){0};
    searchClearButtonRect = (SDL_Rect){0};
    filterHeaderRect = (SDL_Rect){0};
    cachedStamp = 0;
    cachedShowAutoParams = showAutoParamNames;
    cachedShowMacros = showMacros;
    cachedProjectFileCount = 0;
    cachedProjectRoot = NULL;
    free_cached_path();
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
        case CONTROL_FILTER_BTN_MODE_SYMBOLS: return filterMode == CONTROL_FILTER_MODE_SYMBOLS;
        case CONTROL_FILTER_BTN_MODE_METHODS: return filterMode == CONTROL_FILTER_MODE_METHODS;
        case CONTROL_FILTER_BTN_MODE_TYPES: return filterMode == CONTROL_FILTER_MODE_TYPES;
        case CONTROL_FILTER_BTN_MODE_TAGS: return filterMode == CONTROL_FILTER_MODE_TAGS;

        case CONTROL_FILTER_BTN_SCOPE_ACTIVE: return filterScope == CONTROL_FILTER_SCOPE_ACTIVE;
        case CONTROL_FILTER_BTN_SCOPE_OPEN: return filterScope == CONTROL_FILTER_SCOPE_OPEN;
        case CONTROL_FILTER_BTN_SCOPE_PROJECT: return filterScope == CONTROL_FILTER_SCOPE_PROJECT;

        case CONTROL_FILTER_BTN_FIELD_NAME: return (filterFields & CONTROL_FIELD_NAME) != 0;
        case CONTROL_FILTER_BTN_FIELD_TYPE: return (filterFields & CONTROL_FIELD_TYPE) != 0;
        case CONTROL_FILTER_BTN_FIELD_PARAMS: return (filterFields & CONTROL_FIELD_PARAMS) != 0;
        case CONTROL_FILTER_BTN_FIELD_KIND: return (filterFields & CONTROL_FIELD_KIND) != 0;

        case CONTROL_FILTER_BTN_LIVE_PARSE: return isLiveParseEnabled();
        case CONTROL_FILTER_BTN_INLINE_ERRORS: return isShowInlineErrorsEnabled();
        case CONTROL_FILTER_BTN_MACROS: return isShowMacrosEnabled();
        default: return false;
    }
}

void control_panel_activate_filter_button(ControlFilterButtonId id) {
    bool changed = false;
    switch (id) {
        case CONTROL_FILTER_BTN_MODE_SYMBOLS: changed = (filterMode != CONTROL_FILTER_MODE_SYMBOLS); filterMode = CONTROL_FILTER_MODE_SYMBOLS; break;
        case CONTROL_FILTER_BTN_MODE_METHODS: changed = (filterMode != CONTROL_FILTER_MODE_METHODS); filterMode = CONTROL_FILTER_MODE_METHODS; break;
        case CONTROL_FILTER_BTN_MODE_TYPES: changed = (filterMode != CONTROL_FILTER_MODE_TYPES); filterMode = CONTROL_FILTER_MODE_TYPES; break;
        case CONTROL_FILTER_BTN_MODE_TAGS: changed = (filterMode != CONTROL_FILTER_MODE_TAGS); filterMode = CONTROL_FILTER_MODE_TAGS; break;

        case CONTROL_FILTER_BTN_SCOPE_ACTIVE: changed = (filterScope != CONTROL_FILTER_SCOPE_ACTIVE); filterScope = CONTROL_FILTER_SCOPE_ACTIVE; break;
        case CONTROL_FILTER_BTN_SCOPE_OPEN: changed = (filterScope != CONTROL_FILTER_SCOPE_OPEN); filterScope = CONTROL_FILTER_SCOPE_OPEN; break;
        case CONTROL_FILTER_BTN_SCOPE_PROJECT: changed = (filterScope != CONTROL_FILTER_SCOPE_PROJECT); filterScope = CONTROL_FILTER_SCOPE_PROJECT; break;

        case CONTROL_FILTER_BTN_FIELD_NAME: filterFields ^= CONTROL_FIELD_NAME; changed = true; break;
        case CONTROL_FILTER_BTN_FIELD_TYPE: filterFields ^= CONTROL_FIELD_TYPE; changed = true; break;
        case CONTROL_FILTER_BTN_FIELD_PARAMS: filterFields ^= CONTROL_FIELD_PARAMS; changed = true; break;
        case CONTROL_FILTER_BTN_FIELD_KIND: filterFields ^= CONTROL_FIELD_KIND; changed = true; break;

        case CONTROL_FILTER_BTN_LIVE_PARSE: toggleLiveParse(); changed = true; break;
        case CONTROL_FILTER_BTN_INLINE_ERRORS: toggleShowInlineErrors(); changed = true; break;
        case CONTROL_FILTER_BTN_MACROS: toggleShowMacros(); changed = true; break;
        default: break;
    }
    if (changed) {
        control_panel_refresh_visible_symbol_tree();
        reset_symbol_scroll_to_top();
    }
}
