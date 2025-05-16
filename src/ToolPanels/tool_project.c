
#include "tool_project.h"
#include "Render/render_text_helpers.h"
#include "GlobalInfo/core_state.h"
#include "GlobalInfo/project.h"
#include "pane.h"
#include "Editor/editor_view.h"

#include <time.h>


static Uint32 lastClickTime = 0;
static DirEntry* lastClickedEntry = NULL;
int hoveredEntryDepth = 0;     // remove 'static'!

DirEntry* hoveredEntry = NULL;
DirEntry* selectedEntry = NULL;

int mouseX = 0;
int mouseY = 0;



static void handleCommandFolderClick(DirEntry* entry, bool clickedPrefix, bool isDoubleClick);
static void handleCommandOpenFileInEditor(DirEntry* entry);




static void handleCommandFolderClick(DirEntry* entry, bool clickedPrefix, bool isDoubleClick) {
    if (clickedPrefix || isDoubleClick) {
        entry->isExpanded = !entry->isExpanded;
    }
}

static void handleCommandOpenFileInEditor(DirEntry* entry) {
    IDECoreState* core = getCoreState();
    if (core->activeEditorView) {
        printf("[DoubleClick] Appending file: %s to view %p\n", entry->path, (void*)core->activeEditorView);
        openFileInView(core->activeEditorView, entry->path);
    } else {
        fprintf(stderr, "[WARN] No activeEditorView set — cannot open file.\n");
    }
}



void updateHoveredMousePosition(int x, int y) {
    mouseX = x;
    mouseY = y;
}

void handleProjectFilesClick(UIPane* pane, int clickX) {
    if (!hoveredEntry) return;

    Uint32 now = SDL_GetTicks();
    bool isDoubleClick = (hoveredEntry == lastClickedEntry) && (now - lastClickTime < 400);
    lastClickedEntry = hoveredEntry;
    lastClickTime = now;

    selectedEntry = hoveredEntry;

    int indent = hoveredEntryDepth * 20;
    int drawX = pane->x + 12 + indent;
    int prefixWidth = getTextWidth("[-] ");
    bool clickedPrefix = (clickX >= drawX && clickX <= drawX + prefixWidth);

    if (hoveredEntry->type == ENTRY_FOLDER) {
        handleCommandFolderClick(hoveredEntry, clickedPrefix, isDoubleClick);
    } else if (hoveredEntry->type == ENTRY_FILE && isDoubleClick) {
        handleCommandOpenFileInEditor(hoveredEntry);
    }
}


