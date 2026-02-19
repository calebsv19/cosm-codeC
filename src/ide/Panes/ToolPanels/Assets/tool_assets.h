#ifndef TOOL_ASSETS_H
#define TOOL_ASSETS_H

#include "ide/Panes/PaneInfo/pane.h"
#include <SDL2/SDL.h>
#include <stdbool.h>

typedef enum {
    ASSET_CATEGORY_IMAGES = 0,
    ASSET_CATEGORY_AUDIO,
    ASSET_CATEGORY_DATA,
    ASSET_CATEGORY_OTHER,
    ASSET_CATEGORY_COUNT
} AssetCategory;

typedef struct {
    char* name;      // filename only
    char* relPath;   // path relative to workspace
    char* absPath;   // absolute path (for future preview/open)
    AssetCategory category;
} AssetEntry;

typedef struct {
    AssetCategory category;
    AssetEntry* items;
    int count;
    int capacity;
    bool collapsed;
} AssetCategoryList;

typedef struct {
    AssetCategoryList categories[ASSET_CATEGORY_COUNT];
    int totalCount;
} AssetCatalog;

typedef struct {
    const AssetEntry* entry;   // NULL for headers/placeholder
    AssetCategory category;
    int indexInCategory;       // -1 for headers/placeholder
    bool isHeader;
    bool isMoreLine;           // "… and N more" line
} AssetFlatRef;

void initAssetManagerPanel(void);   // rebuild catalog for current workspace
const AssetCatalog* assets_get_catalog(void);

// Flatten visible rows for rendering/hit-testing; returns count.
int assets_flatten(AssetFlatRef* out, int max);

// Selection helpers shared by render/input
bool assets_is_selected(int flatIndex);
void assets_clear_selection(void);
void assets_select_toggle(int flatIndex, bool additive);
void assets_select_range(int a, int b);

// Collapse control
void assets_toggle_collapse(AssetCategory cat);
bool assets_category_collapsed(AssetCategory cat);

// Text-like check for double-click open behavior
bool assets_is_text_like(const AssetEntry* e);

// Persistence (ide_files/assets_catalog.json)
void assets_save_catalog(const char* workspaceRoot);
void assets_load_catalog(const char* workspaceRoot);

void handleAssetManagerEvent(UIPane* pane, SDL_Event* event);

#define ASSET_RENDER_LIMIT_PER_BUCKET 50

#endif
