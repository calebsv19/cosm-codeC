#ifndef TOOL_LIBRARIES_H
#define TOOL_LIBRARIES_H

#include "ide/Panes/PaneInfo/pane.h"
#include "core/Analysis/library_index.h"
#include "ide/UI/interaction_timing.h"
#include "ide/UI/panel_control_widgets.h"
#include "ide/UI/panel_metrics.h"
#include "ide/UI/scroll_manager.h"
#include "core_viewport2d.h"
#include "kit_graph_struct.h"
#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    LIB_NODE_BUCKET = 0,
    LIB_NODE_HEADER,
    LIB_NODE_USAGE,
    LIB_NODE_DEP_SOURCE,
    LIB_NODE_DEP_TARGET
} LibraryNodeType;

typedef enum {
    LIB_PANEL_VIEW_HEADERS = 0,
    LIB_PANEL_VIEW_DEPENDENCIES,
    LIB_PANEL_VIEW_GRAPH
} LibraryPanelViewMode;

typedef struct {
    uint32_t id;
    char* label;
    char* path;
    LibraryNodeType type;
} LibraryGraphNodeInfo;

typedef struct {
    LibraryNodeType type;
    int bucketIndex;
    int headerIndex; // -1 if not applicable
    int usageIndex;  // -1 if not applicable
    int depth;       // for indent
    char* labelPrimary;
    char* labelSecondary; // optional (e.g., resolved path or status)
    LibraryIncludeKind includeKind;
    int usageLine;
    int usageColumn;
    int bucketHeaderCount;
} LibraryFlatRow;

#define LIBRARY_ROW_HEIGHT IDE_UI_DENSE_ROW_HEIGHT
#define LIBRARIES_HEADER_HEIGHT 50
#define LIBRARIES_LIST_TOP_GAP 6

typedef struct {
    int selectedRow;
    int hoveredRow;
    int dragAnchorRow;
    bool selecting;
    UIDoubleClickTracker doubleClickTracker;
    bool bucketExpanded[LIB_BUCKET_COUNT];
    bool* headerExpanded[LIB_BUCKET_COUNT];
    size_t headerExpandedCount[LIB_BUCKET_COUNT];
    PaneScrollState scroll;
    SDL_Rect scrollTrack;
    SDL_Rect scrollThumb;
    LibraryFlatRow* flatRows;
    int flatCount;
    int flatCapacity;
    bool* selected;
    int selectedCapacity;
    bool includeSystemHeaders;
    uint64_t last_published_index_stamp;
    uint64_t last_include_graph_stamp;
    LibraryPanelViewMode viewMode;
    LibraryGraphNodeInfo* graphNodes;
    KitGraphStructEdge* graphEdges;
    KitGraphStructNodeLayout* graphLayouts;
    int graphNodeCount;
    int graphNodeCapacity;
    int graphEdgeCount;
    int graphEdgeCapacity;
    int hiddenGraphNodeCount;
    uint32_t selectedGraphNodeId;
    uint32_t hoveredGraphNodeId;
    CoreViewport2D graphCamera;
    KitGraphStructViewport graphViewport;
    SDL_Rect graphBounds;
    bool graphDragging;
    int graphLastMouseX;
    int graphLastMouseY;
    UIPanelTaggedRect control_hit_storage[3];
    UIPanelTaggedRectList control_hits;
} LibraryPanelState;

typedef enum {
    LIB_TOP_CONTROL_NONE = 0,
    LIB_TOP_CONTROL_SYSTEM_TOGGLE = 1,
    LIB_TOP_CONTROL_LOGS_TOGGLE = 2,
    LIB_TOP_CONTROL_VIEW_MODE = 3
} LibraryTopControlId;

LibraryPanelState* libraries_panel_state(void);

void initLibrariesPanel(void);
void handleLibraryEntryClick(UIPane* pane, int clickX, int clickY, Uint16 modifiers);
void updateHoveredLibraryMousePosition(int x, int y);
bool handleLibraryGraphMouseDown(UIPane* pane, int clickX, int clickY);
void updateLibraryGraphMouseMotion(int x, int y);
void endLibraryGraphDrag(void);
bool handleLibraryGraphWheel(UIPane* pane, SDL_Event* event);
void updateLibraryDragSelection(UIPane* pane, int mouseY);
void endLibrarySelectionDrag(void);
void rebuildLibraryFlatRows(void);
bool library_row_is_selected(int idx);
void copy_selected_rows(void);
void select_all_library_rows(void);
bool handleLibraryHeaderClick(UIPane* pane, int clickX, int clickY);
UIPanelTaggedRectList* libraries_control_hits(void);

#endif // TOOL_LIBRARIES_H
