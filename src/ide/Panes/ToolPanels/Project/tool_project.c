
#include "tool_project.h"
#include "engine/Render/render_text_helpers.h"
#include "engine/Render/render_helpers.h"
#include "app/GlobalInfo/core_state.h"
#include "app/GlobalInfo/project.h"
#include "ide/Panes/PaneInfo/pane.h"
#include "ide/Panes/Editor/editor_view.h"

#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h> // for remove()


static Uint32 lastClickTime = 0;
static DirEntry* lastClickedEntry = NULL;
int hoveredEntryDepth = 0;     // remove 'static'!

DirEntry* hoveredEntry = NULL;
DirEntry* selectedEntry = NULL;
SDL_Rect hoveredEntryRect = {0};

int mouseX = 0;
int mouseY = 0;

// --- Global Button Hitboxes for Project Panel ---
SDL_Rect projectBtnAddFile = {0};
SDL_Rect projectBtnAddFolder = {0};
SDL_Rect projectBtnDelete = {0};


typedef struct {
    char path[1024];
    bool isExpanded;
} DirState;

DirEntry* renamingEntry = NULL;
char renameBuffer[256] = "";

char newlyCreatedPath[1024] = "";

void resetProjectDragState(void) {
    IDECoreState* core = getCoreState();
    ProjectDragState* drag = &core->projectDrag;
    drag->entry = NULL;
    drag->active = false;
    drag->validTarget = false;
    drag->targetView = NULL;
    drag->startX = drag->startY = 0;
    drag->currentX = drag->currentY = 0;
    drag->offsetX = drag->offsetY = 0;
    drag->startTicks = 0;
    setHoveredEditorView(NULL);
}

static void setEditorDropTarget(int mouseX, int mouseY) {
    IDECoreState* core = getCoreState();
    ProjectDragState* drag = &core->projectDrag;
    if (!drag->entry) {
        drag->validTarget = false;
        drag->targetView = NULL;
        setHoveredEditorView(NULL);
        return;
    }

    EditorView* hovered = hitTestLeaf(core->editorViewState, mouseX, mouseY);
    drag->targetView = hovered;
    drag->validTarget = (hovered && hovered->type == VIEW_LEAF);

    setHoveredEditorView(drag->validTarget ? hovered : NULL);
}

void updateHoveredEditorDropTarget(int mouseX, int mouseY) {
    setEditorDropTarget(mouseX, mouseY);
}

void beginProjectDrag(DirEntry* entry, const SDL_Rect* rect, int mouseX, int mouseY) {
    if (!entry || !rect) return;
    resetProjectDragState();
    IDECoreState* core = getCoreState();
    ProjectDragState* drag = &core->projectDrag;
    drag->entry = entry;
    drag->active = false;
    drag->validTarget = false;
    drag->targetView = NULL;
    drag->startX = mouseX;
    drag->startY = mouseY;
    drag->offsetX = mouseX - rect->x;
    drag->offsetY = mouseY - rect->y;
    if (rect->w == 0 && rect->h == 0) {
        drag->offsetX = 10;
        drag->offsetY = 10;
    }
    drag->currentX = mouseX;
    drag->currentY = mouseY;
    drag->startTicks = SDL_GetTicks();
}

void updateProjectDrag(int mouseX, int mouseY) {
    IDECoreState* core = getCoreState();
    ProjectDragState* drag = &core->projectDrag;
    if (!drag->entry) return;

    drag->currentX = mouseX;
    drag->currentY = mouseY;

    if (!drag->active) {
        int dx = abs(mouseX - drag->startX);
        int dy = abs(mouseY - drag->startY);
        if (dx > 5 || dy > 5) {
            drag->active = true;
        } else {
            return;
        }
    }

    setEditorDropTarget(mouseX, mouseY);
}

void finalizeProjectDrag(int mouseX, int mouseY) {
    IDECoreState* core = getCoreState();
    ProjectDragState* drag = &core->projectDrag;
    if (!drag->entry) {
        resetProjectDragState();
        return;
    }

    drag->currentX = mouseX;
    drag->currentY = mouseY;

    if (drag->active) {
        setEditorDropTarget(mouseX, mouseY);
        if (drag->validTarget && drag->targetView) {
            setActiveEditorView(drag->targetView);
            handleCommandOpenFileInEditor(drag->entry);
        }
    }

    resetProjectDragState();
}

void renderProjectDragOverlay(void) {
    IDECoreState* core = getCoreState();
    ProjectDragState* drag = &core->projectDrag;
    if (!drag->entry || !drag->active) return;

    RenderContext* ctx = getRenderContext();
    if (!ctx || !ctx->renderer) return;
    SDL_Renderer* renderer = ctx->renderer;

    const char* base = strrchr(drag->entry->path, '/');
    base = base ? base + 1 : drag->entry->path;
    int textWidth = getTextWidth(base);

    SDL_Rect dragRect = {
        .x = drag->currentX - drag->offsetX,
        .y = drag->currentY - drag->offsetY,
        .w = textWidth + 18,
        .h = 26
    };

    SDL_SetRenderDrawColor(renderer, 36, 36, 36, 245);
    SDL_RenderFillRect(renderer, &dragRect);
    SDL_SetRenderDrawColor(renderer, 210, 210, 210, 255);
    SDL_RenderDrawRect(renderer, &dragRect);
    drawText(dragRect.x + 8, dragRect.y + 5, base);
}


static DirState tempDirStates[2048];
static int tempDirStateCount = 0;

bool pendingProjectRefresh = false;



// 		INITS
// ==============================================
//              HANDLE ...


static void handleCommandFolderClick(DirEntry* entry, bool clickedPrefix, bool isDoubleClick) {
    if (clickedPrefix || isDoubleClick) {
        entry->isExpanded = !entry->isExpanded;
    }
}

void handleCommandOpenFileInEditor(DirEntry* entry) {
    IDECoreState* core = getCoreState();
    if (core->activeEditorView) {
        printf("[DoubleClick] Appending file: %s to view %p\n", entry->path, (void*)core->activeEditorView);
        openFileInView(core->activeEditorView, entry->path);
    } else {
        fprintf(stderr, "[WARN] No activeEditorView set — cannot open %s.\n", entry->path);
    }
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
        if (hoveredEntry->path) {
            handleCommandOpenFileInEditor(hoveredEntry);
        } else {
            fprintf(stderr, "[WARN] hoveredEntry has null path!\n");
        }
    }
}





//		HANDLE ...
// ==============================================
// 		CREATE/DESTROY



void createFileInProject(DirEntry* parent, const char* name) {
    if (!parent || !name) return;

    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", parent->path, name);

    FILE* f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "[ProjectPanel] Failed to create file: %s\n", path);
        return;
    }

    // Write a newline to ensure it's non-empty (optional)
    fputs("\n", f);
    fflush(f);

    // Force write to disk to avoid caching delay before directory rescan
    fsync(fileno(f));
    fclose(f);

    printf("[ProjectPanel] Created file: %s\n", path);
    parent->isExpanded = true;

    strncpy(newlyCreatedPath, path, sizeof(newlyCreatedPath));
    newlyCreatedPath[sizeof(newlyCreatedPath) - 1] = '\0';
}


void createFolderInProject(DirEntry* parent, const char* name) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", parent->path, name);

    if (mkdir(path, 0755) != 0) {
        fprintf(stderr, "[ProjectPanel] Failed to create folder: %s\n", path);
        return;
    }

    printf("[ProjectPanel] Created folder: %s\n", path);
    parent->isExpanded = true;

    strncpy(newlyCreatedPath, path, sizeof(newlyCreatedPath));
    newlyCreatedPath[sizeof(newlyCreatedPath) - 1] = '\0';
}



void deleteSelectedEntry(void) {
    if (!selectedEntry) return;

    DirEntry* parent = selectedEntry->parent;
    if (!parent) {
        fprintf(stderr, "[Delete] Refusing to delete root.\n");
        return;
    }

    int deletedIndex = -1;
    for (int i = 0; i < parent->childCount; i++) {
        if (parent->children[i] == selectedEntry) {
            deletedIndex = i;
            break;
        }
    }

    if (deletedIndex == -1) return;

    // Select next logical entry before deletion
    DirEntry* nextSelection = NULL;
    if (parent->childCount > 1) {
        int newIndex = (deletedIndex < parent->childCount - 1) ? deletedIndex : deletedIndex - 1;
        nextSelection = parent->children[newIndex];
    } else {
        nextSelection = parent;
    }

    // Save its path for post-refresh selection
    if (nextSelection) {
        strncpy(newlyCreatedPath, nextSelection->path, sizeof(newlyCreatedPath));
        newlyCreatedPath[sizeof(newlyCreatedPath) - 1] = '\0';
    }

    // === Remove file/folder from disk ===
    if (selectedEntry->type == ENTRY_FILE) {
        if (remove(selectedEntry->path) != 0) {
            fprintf(stderr, "[Delete] Failed to delete file: %s\n", selectedEntry->path);
        }
    } else if (selectedEntry->type == ENTRY_FOLDER) {
        fprintf(stderr, "[Delete] Folder deletion not supported yet.\n");
        return;
    }

    IDECoreState* core = getCoreState();
    if (selectedEntry->path && core->persistentEditorView) {
       closeFileInAllViews(core->persistentEditorView, selectedEntry->path);
    }

    // Clean up in-memory node if needed — optional
}





//              CREATE/DESTROY   
//   ===================================================
// 		REFRESHING PROJECT







static void cacheExpandedStateRecursive(DirEntry* entry) {
    if (entry->type != ENTRY_FOLDER) return;

    if (tempDirStateCount < 2048) {
        strncpy(tempDirStates[tempDirStateCount].path, entry->path, sizeof(tempDirStates[0].path));
        tempDirStates[tempDirStateCount].path[sizeof(tempDirStates[0].path) - 1] = '\0';
        tempDirStates[tempDirStateCount].isExpanded = entry->isExpanded;
        tempDirStateCount++;
    }

    for (int i = 0; i < entry->childCount; i++) {
        cacheExpandedStateRecursive(entry->children[i]);
    }
}

static bool findCachedExpandedState(const char* path) {
    for (int i = 0; i < tempDirStateCount; i++) {
        if (strcmp(tempDirStates[i].path, path) == 0) {
            return tempDirStates[i].isExpanded;
        }
    }
    return false;
}


static DirEntry* findEntryByPath(DirEntry* root, const char* targetPath) {
    if (!root || !targetPath || strlen(targetPath) == 0) {
        fprintf(stderr, "[findEntryByPath] Invalid inputs — root or path was NULL or empty.\n");
        return NULL;
    }

    if (!root->path) {
        fprintf(stderr, "[findEntryByPath] Root DirEntry has null path (corrupted entry?)\n");
        return NULL;
    }

    if (strcmp(root->path, targetPath) == 0) {
        return root;
    }

    for (int i = 0; i < root->childCount; i++) {
        DirEntry* found = findEntryByPath(root->children[i], targetPath);
        if (found) return found;
    }

    return NULL;
}


static void restoreExpandedStateRecursive(DirEntry* entry) {
    if (entry->type == ENTRY_FOLDER) {
        entry->isExpanded = findCachedExpandedState(entry->path);
        for (int i = 0; i < entry->childCount; i++) {
            restoreExpandedStateRecursive(entry->children[i]);
        }
    }
}


void refreshProjectDirectory(void) {
    tempDirStateCount = 0;

    // Invalidate all UI pointers before refreshing
    selectedEntry = NULL;
    hoveredEntry = NULL;
    renamingEntry = NULL;

    if (projectRoot) {
        cacheExpandedStateRecursive(projectRoot);
        freeDirectory(projectRoot);
    }

    projectRoot = loadProjectDirectory(projectPath);
    restoreExpandedStateRecursive(projectRoot);

    // === NEW: try to restore selection safely
    if (newlyCreatedPath[0] != '\0') {
        DirEntry* restored = findEntryByPath(projectRoot, newlyCreatedPath);
        if (restored) {
            selectedEntry = restored;
        } else {
            fprintf(stderr, "[WARN] Could not restore selected entry: %s\n", newlyCreatedPath);
        }
        newlyCreatedPath[0] = '\0';  // clear
    }
}





//              REFRESHING PROJECT
// ==============================================
// 		HELPERS





void updateHoveredMousePosition(int x, int y) {
    mouseX = x;
    mouseY = y;
}
         



DirEntry* getCurrentTargetDirectory(void) {
    if (selectedEntry) {
        if (selectedEntry->type == ENTRY_FOLDER) {
            return selectedEntry;
        } else if (selectedEntry->type == ENTRY_FILE && selectedEntry->parent) {
            return selectedEntry->parent;
        }
    }

    return projectRoot;
}
