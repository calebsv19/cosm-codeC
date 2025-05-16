#include "project.h"
#include "pane.h" // for UIPane
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h> // POSIX directory reading
#include <sys/stat.h>

DirEntry* projectRoot = NULL;
char projectPath[1024] = {0};

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



DirEntry* loadProjectDirectory(const char* path) {
    DIR* dir = opendir(path);
    if (!dir) {
        printf("Failed to open directory: %s\n", path);
        return NULL;
    }

    DirEntry* root = createDirEntry(path, path, ENTRY_FOLDER);

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
            // Recursively load subdirectory
            DirEntry* childFolder = loadProjectDirectory(fullPath);
            if (childFolder) {
                addChildEntry(root, childFolder);
            }
        } else if (S_ISREG(st.st_mode)) {
            // Regular file
            DirEntry* childFile = createDirEntry(entry->d_name, fullPath, ENTRY_FILE);
            addChildEntry(root, childFile);
        }
    }

    closedir(dir);
    return root;
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

