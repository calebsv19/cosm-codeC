
#include "tool_project.h"
#include "engine/Render/render_text_helpers.h"
#include "engine/Render/render_helpers.h"
#include "app/GlobalInfo/core_state.h"
#include "app/GlobalInfo/project.h"
#include "app/GlobalInfo/workspace_prefs.h"
#include "ide/Panes/Terminal/terminal.h"
#include "ide/Panes/PaneInfo/pane.h"
#include "ide/Panes/Editor/editor_view.h"
#include "core/FileIO/file_ops.h"
#include "core/Analysis/analysis_scheduler.h"
#include "core/Clipboard/clipboard.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h> // for remove()
#include <dirent.h>
#include <errno.h>

static const int PROJECT_TREE_INDENT_WIDTH = 10;
static bool g_projectSelectAllVisible = false;


static Uint32 lastClickTime = 0;
static DirEntry* lastClickedEntry = NULL;
int hoveredEntryDepth = 0;     // remove 'static'!

DirEntry* hoveredEntry = NULL;
DirEntry* selectedEntry = NULL;
DirEntry* selectedFile = NULL;
DirEntry* selectedDirectory = NULL;
SDL_Rect hoveredEntryRect = {0};

int mouseX = 0;
int mouseY = 0;

// --- Global Button Hitboxes for Project Panel ---
SDL_Rect projectBtnAddFile = {0};
SDL_Rect projectBtnDeleteFile = {0};
SDL_Rect projectBtnAddFolder = {0};
SDL_Rect projectBtnDeleteFolder = {0};


typedef struct {
    char path[1024];
    bool isExpanded;
} DirState;

DirEntry* renamingEntry = NULL;
char renameBuffer[256] = "";

char newlyCreatedPath[1024] = "";
char runTargetPath[1024] = "";
static char selectedFilePath[1024] = "";
static char selectedDirectoryPath[1024] = "";

static void clearRunTargetSelection(void);
static void updateRunTargetSelection(DirEntry* entry);
static DirEntry* findEntryByPath(DirEntry* root, const char* targetPath);

static void setSelectedDirectory(DirEntry* entry) {
    selectedDirectory = entry;
    if (entry && entry->path) {
        strncpy(selectedDirectoryPath, entry->path, sizeof(selectedDirectoryPath));
        selectedDirectoryPath[sizeof(selectedDirectoryPath) - 1] = '\0';
    } else {
        selectedDirectoryPath[0] = '\0';
    }
}

static void clearRunTargetSelection(void) {
    runTargetPath[0] = '\0';
    setRunTargetPath(NULL);
    saveRunTargetPreference(NULL);
}

static bool entryIsExecutableFile(const DirEntry* entry) {
    if (!entry || entry->type != ENTRY_FILE || !entry->path) return false;
    struct stat st;
    if (stat(entry->path, &st) != 0) return false;
    return S_ISREG(st.st_mode) && (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH));
}

static void setSelectedFile(DirEntry* entry) {
    selectedFile = entry;
    if (entry && entry->path) {
        strncpy(selectedFilePath, entry->path, sizeof(selectedFilePath));
        selectedFilePath[sizeof(selectedFilePath) - 1] = '\0';
    } else {
        selectedFilePath[0] = '\0';
    }
}

typedef struct ProjectTextBuilder {
    char* data;
    size_t len;
    size_t cap;
} ProjectTextBuilder;

static bool project_builder_reserve(ProjectTextBuilder* b, size_t extra) {
    if (!b) return false;
    size_t need = b->len + extra + 1;
    if (need <= b->cap) return true;
    size_t nextCap = b->cap > 0 ? b->cap : 256;
    while (nextCap < need) nextCap *= 2;
    char* next = (char*)realloc(b->data, nextCap);
    if (!next) return false;
    b->data = next;
    b->cap = nextCap;
    return true;
}

static bool project_builder_append(ProjectTextBuilder* b, const char* text) {
    if (!b || !text) return false;
    size_t n = strlen(text);
    if (!project_builder_reserve(b, n)) return false;
    memcpy(b->data + b->len, text, n);
    b->len += n;
    b->data[b->len] = '\0';
    return true;
}

static bool project_builder_append_char(ProjectTextBuilder* b, char ch) {
    if (!project_builder_reserve(b, 1)) return false;
    b->data[b->len++] = ch;
    b->data[b->len] = '\0';
    return true;
}

static const char* project_display_name(const DirEntry* entry) {
    if (!entry || !entry->path) return "";
    const char* slash = strrchr(entry->path, '/');
    return slash ? (slash + 1) : entry->path;
}

static bool project_snapshot_skip_entry(const DirEntry* entry) {
    if (!entry) return true;
    const char* name = project_display_name(entry);
    if (!name || !name[0]) return true;
    if (entry->parent == NULL) return false;
    if (name[0] == '.' && strcmp(name, "..") != 0) return true;
    if (entry->type == ENTRY_FILE) {
        const char* ext = strrchr(name, '.');
        if (ext && (strcmp(ext, ".o") == 0 ||
                    strcmp(ext, ".obj") == 0 ||
                    strcmp(ext, ".out") == 0)) {
            return true;
        }
        if (strcmp(name, "last_build") == 0) return true;
    }
    return false;
}

static bool project_append_visible_entry(ProjectTextBuilder* b, const DirEntry* entry, int depth) {
    if (!b || !entry) return true;
    if (project_snapshot_skip_entry(entry)) return true;

    const char* name = project_display_name(entry);
    for (int i = 0; i < depth; ++i) {
        if (!project_builder_append(b, "  ")) return false;
    }
    if (entry->type == ENTRY_FOLDER) {
        if (!project_builder_append(b, entry->isExpanded ? "[-] " : "[+] ")) return false;
    }
    if (!project_builder_append(b, name ? name : "")) return false;
    if (!project_builder_append_char(b, '\n')) return false;

    if (entry->type == ENTRY_FOLDER && entry->isExpanded) {
        for (int i = 0; i < entry->childCount; ++i) {
            if (!project_append_visible_entry(b, entry->children[i], depth + 1)) return false;
        }
    }
    return true;
}

void project_select_all_visible_entries(void) {
    g_projectSelectAllVisible = true;
}

bool project_select_all_visual_active(void) {
    return g_projectSelectAllVisible;
}

void project_clear_select_all_visual(void) {
    g_projectSelectAllVisible = false;
}

bool project_copy_visible_entries_to_clipboard(void) {
    if (!projectRoot) return false;
    if (!g_projectSelectAllVisible && hoveredEntry) {
        const char* name = project_display_name(hoveredEntry);
        char line[1024];
        if (hoveredEntry->type == ENTRY_FOLDER) {
            snprintf(line, sizeof(line), "%s%s",
                     hoveredEntry->isExpanded ? "[-] " : "[+] ",
                     name ? name : "");
        } else {
            snprintf(line, sizeof(line), "%s", name ? name : "");
        }
        return clipboard_copy_text(line);
    }

    ProjectTextBuilder b = {0};
    if (!project_builder_reserve(&b, 512)) return false;
    b.data[0] = '\0';
    if (!project_append_visible_entry(&b, projectRoot, 0) || b.len == 0) {
        free(b.data);
        return false;
    }
    g_projectSelectAllVisible = false;
    bool ok = clipboard_copy_text(b.data);
    free(b.data);
    return ok;
}

void selectDirectoryEntry(DirEntry* entry) {
    setSelectedDirectory(entry);
    selectedEntry = entry;
    setSelectedFile(NULL);
    if (entry) {
        updateRunTargetSelection(entry);
    }
}

void selectFileEntry(DirEntry* entry) {
    setSelectedFile(entry);
    if (entry) {
        selectedEntry = entry;
        if ((!selectedDirectory || selectedDirectory == projectRoot) && entry->parent) {
            setSelectedDirectory(entry->parent);
        }
        updateRunTargetSelection(entry);
    }
}

static DirEntry* findNewestExecutableRecursive(DirEntry* entry, time_t* newestTime) {
    if (!entry) return NULL;
    DirEntry* best = NULL;

    if (entry->type == ENTRY_FILE && entry->path) {
        struct stat st;
        if (stat(entry->path, &st) == 0) {
            if (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
                if (st.st_mtime >= *newestTime) {
                    *newestTime = st.st_mtime;
                    best = entry;
                }
            }
        }
    }

    if (entry->type == ENTRY_FOLDER) {
        for (int i = 0; i < entry->childCount; ++i) {
            DirEntry* candidate = findNewestExecutableRecursive(entry->children[i], newestTime);
            if (candidate) {
                best = candidate;
            }
        }
    }

    return best;
}

static void updateRunTargetSelection(DirEntry* entry) {
    if (!entry) {
        clearRunTargetSelection();
        return;
    }

    if (entry->type == ENTRY_FILE) {
        if (!entryIsExecutableFile(entry)) {
            clearRunTargetSelection();
            return;
        }
        if (strcmp(runTargetPath, entry->path) != 0) {
            snprintf(runTargetPath, sizeof(runTargetPath), "%s", entry->path);
            setRunTargetPath(runTargetPath);
            saveRunTargetPreference(runTargetPath);
        }
    } else {
        time_t newestTime = 0;
        DirEntry* newest = findNewestExecutableRecursive(entry, &newestTime);

        if (newest && newest->path) {
            if (strcmp(runTargetPath, newest->path) != 0) {
                snprintf(runTargetPath, sizeof(runTargetPath), "%s", newest->path);
                setRunTargetPath(runTargetPath);
                saveRunTargetPreference(runTargetPath);
            }
        } else {
            clearRunTargetSelection();
        }
    }
}

void restoreRunTargetSelection(void) {
    const char* saved = getRunTargetPath();
    if (!saved || !saved[0]) {
        runTargetPath[0] = '\0';
        return;
    }

    DirEntry* entry = findEntryByPath(projectRoot, saved);
    if (entry) {
        snprintf(runTargetPath, sizeof(runTargetPath), "%s", entry->path);
    } else {
        clearRunTargetSelection();
    }
}

static void clearProjectDirectoryDropTarget(ProjectDragState* drag) {
    if (!drag) return;
    drag->targetDirectory = NULL;
    drag->validDirectoryTarget = false;
    drag->targetDirectoryRect = (SDL_Rect){0};
}

static void setProjectDirectoryDropTarget(ProjectDragState* drag) {
    if (!drag) return;
    drag->targetDirectory = NULL;
    drag->validDirectoryTarget = false;
    drag->targetDirectoryRect = (SDL_Rect){0};
    if (!drag->active || !drag->entry || drag->entry->type != ENTRY_FILE) return;
    if (!hoveredEntry || hoveredEntry->type != ENTRY_FOLDER) return;
    if (!hoveredEntry->path || !drag->entry->name) return;
    if (hoveredEntry == drag->entry->parent) return;

    char destPath[1024];
    snprintf(destPath, sizeof(destPath), "%s/%s", hoveredEntry->path, drag->entry->name);
    struct stat st;
    if (stat(destPath, &st) == 0) return;

    drag->targetDirectory = hoveredEntry;
    drag->validDirectoryTarget = true;
    drag->targetDirectoryRect = hoveredEntryRect;
}

static bool moveFileEntryToDirectory(DirEntry* fileEntry, DirEntry* destDir) {
    if (!fileEntry || fileEntry->type != ENTRY_FILE || !fileEntry->path || !fileEntry->name) return false;
    if (!destDir || destDir->type != ENTRY_FOLDER || !destDir->path) return false;

    char destPath[1024];
    snprintf(destPath, sizeof(destPath), "%s/%s", destDir->path, fileEntry->name);
    if (strcmp(fileEntry->path, destPath) == 0) return false;
    if (!renameFileOnDisk(fileEntry->path, destPath)) return false;

    fileEntry->parent = destDir;
    free(fileEntry->path);
    fileEntry->path = strdup(destPath);

    destDir->isExpanded = true;
    strncpy(newlyCreatedPath, destPath, sizeof(newlyCreatedPath));
    newlyCreatedPath[sizeof(newlyCreatedPath) - 1] = '\0';
    queueProjectRefresh(ANALYSIS_REASON_PROJECT_MUTATION);
    return true;
}

void resetProjectDragState(void) {
    IDECoreState* core = getCoreState();
    ProjectDragState* drag = &core->projectDrag;
    drag->entry = NULL;
    drag->active = false;
    drag->validTarget = false;
    drag->targetView = NULL;
    clearProjectDirectoryDropTarget(drag);
    drag->startX = drag->startY = 0;
    drag->currentX = drag->currentY = 0;
    drag->offsetX = drag->offsetY = 0;
    drag->labelWidth = 0;
    drag->cachedLabel[0] = '\0';
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
    drag->cachedLabel[0] = '\0';
    drag->labelWidth = 0;
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
    setProjectDirectoryDropTarget(drag);
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
        setProjectDirectoryDropTarget(drag);
        if (drag->validTarget && drag->targetView) {
            setActiveEditorView(drag->targetView);
            handleCommandOpenFileInEditor(drag->entry);
        } else if (drag->validDirectoryTarget && drag->targetDirectory) {
            moveFileEntryToDirectory(drag->entry, drag->targetDirectory);
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

    if (!drag->cachedLabel[0]) {
        const char* base = strrchr(drag->entry->path, '/');
        base = base ? base + 1 : drag->entry->path;
        strncpy(drag->cachedLabel, base, sizeof(drag->cachedLabel) - 1);
        drag->cachedLabel[sizeof(drag->cachedLabel) - 1] = '\0';
        drag->labelWidth = getTextWidth(drag->cachedLabel);
    }

    SDL_Rect dragRect = {
        .x = drag->currentX - drag->offsetX,
        .y = drag->currentY - drag->offsetY,
        .w = drag->labelWidth + 24,
        .h = 28
    };

    SDL_Rect shadowRect = dragRect;
    shadowRect.x += 2;
    shadowRect.y += 3;
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 110);
    SDL_RenderFillRect(renderer, &shadowRect);

    SDL_SetRenderDrawColor(renderer, 40, 40, 40, 235);
    SDL_RenderFillRect(renderer, &dragRect);
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
    SDL_RenderDrawRect(renderer, &dragRect);
    drawText(dragRect.x + 10, dragRect.y + 6, drag->cachedLabel);
}


static DirState tempDirStates[2048];
static int tempDirStateCount = 0;

bool pendingProjectRefresh = false;
unsigned int pendingProjectRefreshReasonMask = 0;

void queueProjectRefresh(unsigned int analysisReasonMask) {
    pendingProjectRefresh = true;
    pendingProjectRefreshReasonMask |= analysisReasonMask;
}



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
        openFileInView(core->activeEditorView, entry->path);
    } else {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "[Project] No active editor view to open %s", entry->path);
    }
}

void handleProjectFilesClick(UIPane* pane, int clickX) {
    if (!hoveredEntry) return;

    Uint32 now = SDL_GetTicks();
    bool isDoubleClick = (hoveredEntry == lastClickedEntry) && (now - lastClickTime < 400);
    lastClickedEntry = hoveredEntry;
    lastClickTime = now;

    int indent = hoveredEntryDepth * PROJECT_TREE_INDENT_WIDTH;
    int drawX = pane->x + 12 + indent;
    int prefixWidth = getTextWidth("[-] ");
    bool clickedPrefix = (clickX >= drawX && clickX <= drawX + prefixWidth);

    if (hoveredEntry->type == ENTRY_FOLDER) {
        selectDirectoryEntry(hoveredEntry);
        strncpy(renameBuffer, hoveredEntry->name, sizeof(renameBuffer));
        renameBuffer[sizeof(renameBuffer) - 1] = '\0';
        handleCommandFolderClick(hoveredEntry, clickedPrefix, isDoubleClick);
    } else if (hoveredEntry->type == ENTRY_FILE && isDoubleClick) {
        selectFileEntry(hoveredEntry);
        if (hoveredEntry->path) {
            handleCommandOpenFileInEditor(hoveredEntry);
        } else {
            fprintf(stderr, "[WARN] hoveredEntry has null path!\n");
        }
    } else if (hoveredEntry->type == ENTRY_FILE) {
        selectFileEntry(hoveredEntry);
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



static bool deletePathRecursive(const char* path) {
    if (!path) return false;

    struct stat st;
    if (lstat(path, &st) != 0) {
        fprintf(stderr, "[Delete] Failed to stat %s: %s\n", path, strerror(errno));
        return false;
    }

    if (S_ISDIR(st.st_mode)) {
        DIR* dir = opendir(path);
        if (!dir) {
            fprintf(stderr, "[Delete] Failed to open dir %s: %s\n", path, strerror(errno));
            return false;
        }

        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }

            char childPath[1024];
            snprintf(childPath, sizeof(childPath), "%s/%s", path, entry->d_name);
            if (!deletePathRecursive(childPath)) {
                closedir(dir);
                return false;
            }
        }

        closedir(dir);
        if (rmdir(path) != 0) {
            fprintf(stderr, "[Delete] Failed to remove dir %s: %s\n", path, strerror(errno));
            return false;
        }
        return true;
    }

    if (remove(path) != 0) {
        fprintf(stderr, "[Delete] Failed to delete %s: %s\n", path, strerror(errno));
        return false;
    }
    return true;
}

static void closeFilesRecursive(EditorView* view, DirEntry* entry) {
    if (!view || !entry) return;

    if (entry->type == ENTRY_FILE) {
        if (entry->path) {
            closeFileInAllViews(view, entry->path);
        }
        return;
    }

    for (int i = 0; i < entry->childCount; ++i) {
        closeFilesRecursive(view, entry->children[i]);
    }
}

void deleteSelectedFile(void) {
    if (!selectedFile || !selectedFile->path) return;

    DirEntry* fileEntry = selectedFile;
    DirEntry* parent = fileEntry->parent;

    if (remove(fileEntry->path) != 0) {
        fprintf(stderr, "[Delete] Failed to delete file: %s (%s)\n", fileEntry->path, strerror(errno));
        return;
    }

    if (runTargetPath[0] && strcmp(runTargetPath, fileEntry->path) == 0) {
        clearRunTargetSelection();
    }

    IDECoreState* core = getCoreState();
    if (core->persistentEditorView) {
        closeFileInAllViews(core->persistentEditorView, fileEntry->path);
    }

    setSelectedFile(NULL);
    if (parent) {
        selectDirectoryEntry(parent);
        selectedDirectory = parent;
        if (parent->path) {
            strncpy(selectedDirectoryPath, parent->path, sizeof(selectedDirectoryPath));
            selectedDirectoryPath[sizeof(selectedDirectoryPath) - 1] = '\0';
        }
    } else {
        selectedEntry = NULL;
    }

    queueProjectRefresh(ANALYSIS_REASON_PROJECT_MUTATION);
}

void deleteSelectedDirectory(void) {
    if (!selectedDirectory || selectedDirectory == projectRoot || !selectedDirectory->path) {
        fprintf(stderr, "[Delete] No deletable directory selected.\n");
        return;
    }

    DirEntry* dirEntry = selectedDirectory;
    DirEntry* parent = dirEntry->parent;

    IDECoreState* core = getCoreState();
    if (core->persistentEditorView) {
        closeFilesRecursive(core->persistentEditorView, dirEntry);
    }

    if (!deletePathRecursive(dirEntry->path)) {
        fprintf(stderr, "[Delete] Failed to remove directory tree: %s\n", dirEntry->path);
        return;
    }

    if (runTargetPath[0]) {
        size_t dirLen = strlen(dirEntry->path);
        if (strncmp(runTargetPath, dirEntry->path, dirLen) == 0) {
            char next = runTargetPath[dirLen];
            if (next == '\0' || next == '/' || next == '\\') {
                clearRunTargetSelection();
            }
        }
    }

    setSelectedFile(NULL);
    if (parent) {
        selectDirectoryEntry(parent);
    } else {
        selectDirectoryEntry(projectRoot);
    }

    queueProjectRefresh(ANALYSIS_REASON_PROJECT_MUTATION);
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
        if (entry->parent == NULL) {
            entry->isExpanded = true;
        } else {
            entry->isExpanded = findCachedExpandedState(entry->path);
        }
        for (int i = 0; i < entry->childCount; i++) {
            restoreExpandedStateRecursive(entry->children[i]);
        }
    }
}


void refreshProjectDirectory(void) {
    tempDirStateCount = 0;

    // Invalidate all UI pointers before refreshing
    selectedEntry = NULL;
    selectedFile = NULL;
    selectedDirectory = NULL;
    hoveredEntry = NULL;
    renamingEntry = NULL;

    if (projectRoot) {
        cacheExpandedStateRecursive(projectRoot);
        freeDirectory(projectRoot);
    }

    projectRoot = loadProjectDirectory(projectPath);
    restoreExpandedStateRecursive(projectRoot);

    // === Restore selections ===
    if (newlyCreatedPath[0] != '\0') {
        DirEntry* restored = findEntryByPath(projectRoot, newlyCreatedPath);
        if (restored) {
            if (restored->type == ENTRY_FOLDER) {
                setSelectedDirectory(restored);
            } else {
                setSelectedFile(restored);
                if (restored->parent) setSelectedDirectory(restored->parent);
            }
            selectedEntry = restored;
        } else {
            fprintf(stderr, "[WARN] Could not restore created entry: %s\n", newlyCreatedPath);
        }
        newlyCreatedPath[0] = '\0';
    } else {
        if (selectedFilePath[0] != '\0') {
            DirEntry* restoredFile = findEntryByPath(projectRoot, selectedFilePath);
            if (restoredFile && restoredFile->type == ENTRY_FILE) {
                setSelectedFile(restoredFile);
            } else {
                selectedFilePath[0] = '\0';
            }
        }

        if (selectedDirectoryPath[0] != '\0') {
            DirEntry* restoredDir = findEntryByPath(projectRoot, selectedDirectoryPath);
            if (restoredDir && restoredDir->type == ENTRY_FOLDER) {
                setSelectedDirectory(restoredDir);
            } else {
                selectedDirectoryPath[0] = '\0';
            }
        }

        if (!selectedDirectory && projectRoot) {
            setSelectedDirectory(projectRoot);
        }

        if (selectedFile) {
            selectedEntry = selectedFile;
        } else {
            selectedEntry = selectedDirectory;
        }
    }

    // Do not restart terminals here; we want build/run sessions to keep their scrollback.
    // If we ever need to start a shell on first load, do it from initTerminal instead.

    restoreRunTargetSelection();
}





//              REFRESHING PROJECT
// ==============================================
// 		HELPERS





void updateHoveredMousePosition(int x, int y) {
    mouseX = x;
    mouseY = y;
}
         



DirEntry* getCurrentTargetDirectory(void) {
    if (selectedDirectory) {
        return selectedDirectory;
    }

    if (selectedFile && selectedFile->parent) {
        return selectedFile->parent;
    }

    return projectRoot;
}
