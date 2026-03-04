#ifndef TOOL_ASSETS_H
#define TOOL_ASSETS_H

#include "ide/Panes/PaneInfo/pane.h"
#include "ide/UI/flat_list_interaction.h"
#include "ide/UI/interaction_timing.h"
#include "ide/UI/panel_control_widgets.h"
#include "ide/UI/panel_metrics.h"
#include "ide/UI/scroll_manager.h"
#include <SDL2/SDL.h>
#include <stdbool.h>

typedef enum {
    ASSET_CATEGORY_IMAGES = 0,
    ASSET_CATEGORY_AUDIO,
    ASSET_CATEGORY_DATA,
    ASSET_CATEGORY_DOCS,
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

typedef enum {
    ASSET_TOP_CONTROL_NONE = 0,
    ASSET_TOP_CONTROL_OPEN_ALL = 1,
    ASSET_TOP_CONTROL_CLOSE_ALL = 2
} AssetTopControlId;

void initAssetManagerPanel(void);   // rebuild catalog for current workspace
const AssetCatalog* assets_get_catalog(void);

// Flatten visible rows for rendering/hit-testing; returns count.
int assets_flatten(AssetFlatRef* out, int max);

// Selection helpers shared by render/input
bool assets_is_selected(int flatIndex);
void assets_clear_selection(void);
void assets_select_toggle(int flatIndex, bool additive);
void assets_select_range(int a, int b);
void assets_select_all_visible(void);
bool assets_copy_selection_to_clipboard(void);

// Collapse control
void assets_toggle_collapse(AssetCategory cat);
bool assets_category_collapsed(AssetCategory cat);
void assets_set_all_collapsed(bool collapsed);

// Text-like check for double-click open behavior
bool assets_is_text_like(const AssetEntry* e);

// Persistence (ide_files/assets_catalog.json)
void assets_save_catalog(const char* workspaceRoot);
void assets_load_catalog(const char* workspaceRoot);

PaneScrollState* assets_get_scroll_state(UIPane* pane);
SDL_Rect assets_get_scroll_track_rect(void);
SDL_Rect assets_get_scroll_thumb_rect(void);
void assets_set_scroll_rects(SDL_Rect track, SDL_Rect thumb);
UIPanelTaggedRectList* assets_get_control_hits(void);
UIDoubleClickTracker* assets_get_double_click_tracker(void);
UIFlatListDragState* assets_get_drag_state(void);

void handleAssetManagerEvent(UIPane* pane, SDL_Event* event);

#define ASSET_PANEL_ROW_HEIGHT IDE_UI_DENSE_ROW_HEIGHT
#define ASSET_PANEL_HEADER_HEIGHT IDE_UI_DENSE_HEADER_HEIGHT
#define ASSET_RENDER_LIMIT_PER_BUCKET 50

#endif
