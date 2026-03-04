#ifndef TOOL_LIBRARIES_H
#define TOOL_LIBRARIES_H

#include "ide/Panes/PaneInfo/pane.h"
#include "core/Analysis/library_index.h"
#include "ide/UI/interaction_timing.h"
#include "ide/UI/panel_control_widgets.h"
#include "ide/UI/panel_metrics.h"
#include "ide/UI/scroll_manager.h"
#include <SDL2/SDL.h>
#include <stdbool.h>

typedef enum {
    LIB_NODE_BUCKET = 0,
    LIB_NODE_HEADER,
    LIB_NODE_USAGE
} LibraryNodeType;

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
    UIPanelTaggedRect control_hit_storage[2];
    UIPanelTaggedRectList control_hits;
} LibraryPanelState;

typedef enum {
    LIB_TOP_CONTROL_NONE = 0,
    LIB_TOP_CONTROL_SYSTEM_TOGGLE = 1,
    LIB_TOP_CONTROL_LOGS_TOGGLE = 2
} LibraryTopControlId;

LibraryPanelState* libraries_panel_state(void);

void initLibrariesPanel(void);
void handleLibraryEntryClick(UIPane* pane, int clickX, int clickY, Uint16 modifiers);
void updateHoveredLibraryMousePosition(int x, int y);
void updateLibraryDragSelection(UIPane* pane, int mouseY);
void endLibrarySelectionDrag(void);
void rebuildLibraryFlatRows(void);
bool library_row_is_selected(int idx);
void copy_selected_rows(void);
void select_all_library_rows(void);
bool handleLibraryHeaderClick(UIPane* pane, int clickX, int clickY);
UIPanelTaggedRectList* libraries_control_hits(void);

#endif // TOOL_LIBRARIES_H
