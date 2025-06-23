#include "tool_libraries.h"
#include "Render/render_text_helpers.h"
#include "GlobalInfo/project.h"
#include "GlobalInfo/core_state.h"
#include "Editor/editor_view.h"


DirEntry* libraryRoot = NULL;


DirEntry* hoveredLibraryEntry = NULL;
DirEntry* selectedLibraryEntry = NULL;
int hoveredLibraryDepth = 0;

int libraryMouseX = 0;
int libraryMouseY = 0;
static Uint32 lastLibraryClickTime = 0;
static DirEntry* lastClickedLibrary = NULL;

void initLibrariesPanel() {
    if (libraryRoot) freeDirectory(libraryRoot);
    libraryRoot = scanDirectory("src/Libraries", 0);
}






static void handleCommandToggleLibraryFolder(DirEntry* entry, bool clickedPrefix, bool isDoubleClick) {
    if (clickedPrefix || isDoubleClick) {
        entry->isExpanded = !entry->isExpanded;
    }
}

static void handleCommandOpenLibraryFile(DirEntry* entry) {
    IDECoreState* core = getCoreState();
    if (core->activeEditorView) {
        printf("[Library Click] Opening: %s\n", entry->path);
        openFileInView(core->activeEditorView, entry->path);
    }
}

void updateHoveredLibraryMousePosition(int x, int y) {
    libraryMouseX = x;
    libraryMouseY = y;
}




void handleLibraryEntryClick(UIPane* pane, int clickX) {
    if (!hoveredLibraryEntry) return;

    Uint32 now = SDL_GetTicks();
    bool isDoubleClick = (hoveredLibraryEntry == lastClickedLibrary) &&
                         (now - lastLibraryClickTime < 400);
    lastClickedLibrary = hoveredLibraryEntry;
    lastLibraryClickTime = now;

    selectedLibraryEntry = hoveredLibraryEntry;

    int indent = hoveredLibraryDepth * 20;
    int drawX = pane->x + 12 + indent;
    int prefixWidth = getTextWidth("[-] ");
    bool clickedPrefix = (clickX >= drawX && clickX <= drawX + prefixWidth);

    if (hoveredLibraryEntry->type == ENTRY_FOLDER) {
        handleCommandToggleLibraryFolder(hoveredLibraryEntry, clickedPrefix, isDoubleClick);
    } else if (hoveredLibraryEntry->type == ENTRY_FILE && isDoubleClick) {
        handleCommandOpenLibraryFile(hoveredLibraryEntry);
    }
}






