
#include "tool_project.h"
#include "Render/render_text_helpers.h"
#include "GlobalInfo/core_state.h"
#include "GlobalInfo/project.h"
#include "PaneInfo/pane.h"
#include "Editor/editor_view.h"

#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h> // for remove()


static Uint32 lastClickTime = 0;
static DirEntry* lastClickedEntry = NULL;
int hoveredEntryDepth = 0;     // remove 'static'!

DirEntry* hoveredEntry = NULL;
DirEntry* selectedEntry = NULL;

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

static void handleCommandOpenFileInEditor(DirEntry* entry) {
    IDECoreState* core = getCoreState();
    if (core->activeEditorView) {
        printf("[DoubleClick] Appending file: %s to view %p\n", entry->path, (void*)core->activeEditorView);
        openFileInView(core->activeEditorView, entry->path);
    } else {
        fprintf(stderr, "[WARN] No activeEditorView set — cannot open file.\n");
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
