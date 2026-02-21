#ifndef TOOL_LIBRARIES_H
#define TOOL_LIBRARIES_H

#include "ide/Panes/PaneInfo/pane.h"
#include "core/Analysis/library_index.h"
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

#define LIBRARY_ROW_HEIGHT 20
#define LIBRARIES_HEADER_HEIGHT 50
#define LIBRARIES_LIST_TOP_GAP 6

typedef struct {
    int selectedRow;
    int hoveredRow;
    int dragAnchorRow;
    bool selecting;
    Uint32 lastClickTicks;
    int lastClickedRow;
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
    SDL_Rect systemToggleRect;
} LibraryPanelState;

extern LibraryPanelState g_libraryPanelState;

void initLibrariesPanel(void);
void handleLibraryEntryClick(UIPane* pane, int clickX, int clickY);
void updateHoveredLibraryMousePosition(int x, int y);
void updateLibraryDragSelection(UIPane* pane, int mouseY);
void endLibrarySelectionDrag(void);
void rebuildLibraryFlatRows(void);
bool library_row_is_selected(int idx);
void copy_selected_rows(void);
bool handleLibraryHeaderClick(UIPane* pane, int clickX, int clickY);

#endif // TOOL_LIBRARIES_H
