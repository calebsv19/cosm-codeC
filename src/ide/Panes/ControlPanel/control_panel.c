#include "control_panel.h"

#include <stdlib.h>
#include <string.h>

#include "core/Analysis/analysis_symbols_store.h"
#include "ide/Panes/ControlPanel/symbol_tree_adapter.h"
#include "ide/UI/Trees/ui_tree_node.h"
#include "ide/Panes/PaneInfo/pane.h"
#include "app/GlobalInfo/project.h"

static bool liveParseEnabled = false;
static bool showInlineErrors = false;
static bool showAutoParamNames = false;
static bool showMacros = false;

static UITreeNode* symbolTree = NULL;
static PaneScrollState symbolScroll;
static bool symbolScrollInit = false;
static SDL_Rect symbolScrollTrack = {0};
static SDL_Rect symbolScrollThumb = {0};
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

void control_panel_reset_symbol_tree(void) {
    if (symbolTree) {
        freeTreeNodeRecursive(symbolTree);
        symbolTree = NULL;
    }
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

    if (symbolTree) {
        freeTreeNodeRecursive(symbolTree);
        symbolTree = NULL;
    }

    symbolTree = buildSymbolTreeForWorkspace(projectRoot, filePath, showAutoParamNames, showMacros);
    cachedStamp = stamp;
    cachedShowAutoParams = showAutoParamNames;
    cachedShowMacros = showMacros;
    cachedProjectFileCount = projectFileCount;
    cachedProjectRoot = projectRoot;

    free_cached_path();
    cachedFilePath = filePath ? strdup(filePath) : NULL;

    if (fileChanged && symbolScrollInit) {
        symbolScroll.offset_px = 0.0f;
        symbolScroll.target_offset_px = 0.0f;
    }
}

UITreeNode* control_panel_get_symbol_tree(void) {
    return symbolTree;
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
    if (!pane) return 0;
    const int padding = 8;
    int y = pane->y + padding;
    y += 28; // title
    y += 20; // search label
    y += 36; // search box
    y += 24; // options label
    y += 20; // live parse
    y += 20; // inline errors
    y += 20; // show macros
    y += 16; // spacing
    return y;
}
