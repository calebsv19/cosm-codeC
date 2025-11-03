#include "project.h"
#include "core_state.h"
#include "workspace_prefs.h"
#include "core/Watcher/file_watcher.h"
#include "ide/Panes/PaneInfo/pane.h" // for UIPane
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h> // POSIX directory reading
#include <sys/stat.h>
#include <unistd.h>

DirEntry* projectRoot = NULL;
char projectPath[1024] = {0};
char projectRootPath[1024] = {0};


void initProjectPaths(void) {
    // Set project root (IDE directory) to current working directory
    getcwd(projectRootPath, sizeof(projectRootPath));


    // Set test project to ~/Desktop/Project
    const char* defaultProjectPath = "/Users/calebsv16/Desktop/Project";
    const char* persistedPath = loadWorkspacePreference();
    if (persistedPath && persistedPath[0] != '\0') {
        setWorkspacePath(persistedPath);
    }

    const char* chosenPath = getWorkspacePath();
    if (!chosenPath || chosenPath[0] == '\0') {
        chosenPath = defaultProjectPath;
    }

    snprintf(projectPath, sizeof(projectPath), "%s", chosenPath);
    setWorkspacePath(projectPath);
    setWorkspaceWatchPath(projectPath);

}




// --- Internal Helpers ---

static DirEntry* createDirEntry(const char* name, const char* path, EntryType type) {
    DirEntry* entry = (DirEntry*)malloc(sizeof(DirEntry));
    if (!entry) return NULL;

    entry->type = type;
    entry->name = strdup(name);   // Copy string into heap memory
    entry->path = strdup(path);   // Copy string into heap memory

    entry->children = NULL;
    entry->childCount = 0;
    entry->childCapacity = 0;

    return entry;
}

static void addChildEntry(DirEntry* parent, DirEntry* child) {
    if (!parent || !child || parent->type != ENTRY_FOLDER) return;

    if (parent->childCount >= parent->childCapacity) {
        parent->childCapacity = (parent->childCapacity == 0) ? 4 : parent->childCapacity * 2;
        parent->children = realloc(parent->children, sizeof(DirEntry*) * parent->childCapacity);
    }

    parent->children[parent->childCount++] = child;
}






// --- Public Functions ---



DirEntry* scanDirectory(const char* path, int depth) {
    // depth parameter is ignored for now; added for future compatibility
    (void)depth;  // suppress unused warning
    return loadProjectDirectory(path);
}

// Alias for freeing any loaded directory (project or otherwise)
void freeDirectory(DirEntry* root) {
    destroyProjectDirectory(root);
}



static DirEntry* loadProjectDirectoryInternal(const char* path, DirEntry* parent) {
    DIR* dir = opendir(path);
    if (!dir) {
        printf("Failed to open directory: %s\n", path);
        return NULL;
    }

    const char* baseName = strrchr(path, '/');
    baseName = (baseName && *(baseName + 1)) ? baseName + 1 : path;

    DirEntry* root = createDirEntry(baseName, path, ENTRY_FOLDER);
    if (!root) {
        closedir(dir);
        return NULL;
    }

    root->parent = parent;
    if (parent == NULL) {
        root->isExpanded = true;
    } else {
        const char* parentPath = parent->path;
        const char* workspace = getWorkspacePath();
        if (workspace && strncmp(path, workspace, strlen(workspace)) == 0) {
            const char* relative = path + strlen(workspace);
            if (relative[0] == '/' || relative[0] == '\\') relative++;
            if (strncmp(relative, "BuildOutputs", strlen("BuildOutputs")) == 0) {
                root->isExpanded = true;
            }
        }
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        // Build full path
        char fullPath[1024];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", path, entry->d_name);

        struct stat st;
        if (stat(fullPath, &st) == -1) {
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            DirEntry* childFolder = loadProjectDirectoryInternal(fullPath, root);
            if (childFolder) {
                addChildEntry(root, childFolder);
            }
        } else if (S_ISREG(st.st_mode)) {
            DirEntry* childFile = createDirEntry(entry->d_name, fullPath, ENTRY_FILE);
            if (childFile) {
                childFile->parent = root;
                addChildEntry(root, childFile);
            }
        }
    }

    closedir(dir);
    return root;
}




DirEntry* loadProjectDirectory(const char* path) {
    return loadProjectDirectoryInternal(path, NULL);
}





void destroyProjectDirectory(DirEntry* entry) {
    if (!entry) return;
    
    // Recursively free children
    for (int i = 0; i < entry->childCount; i++) {
        destroyProjectDirectory(entry->children[i]);
    }

    free(entry->children);
    free(entry->name);
    free(entry->path);
    free(entry);
}

void renderProjectTree(struct UIPane* pane, SDL_Renderer* renderer, DirEntry* root) {
    if (!root) return;

    // Start rendering tree from root at some x,y offset
    // Later indent each child based on depth
}

void handleProjectTreeClick(int mouseX, int mouseY) {
    // Detect if the user clicked on a file entry
    // Then open file into EditorView
}
