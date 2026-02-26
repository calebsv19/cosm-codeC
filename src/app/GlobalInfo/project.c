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
#include <errno.h>

DirEntry* projectRoot = NULL;
char projectPath[1024] = {0};
char projectRootPath[1024] = {0};

static bool project_loader_debug_enabled(void) {
    const char* env = getenv("IDE_PROJECT_LOADER_DEBUG");
    return (env && env[0] == '1');
}


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

    setWorkspacePath(chosenPath);
    snprintf(projectPath, sizeof(projectPath), "%s", getWorkspacePath());
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

static int entry_priority_root(const DirEntry* entry) {
    if (!entry || !entry->name) return 3;
    if (strcmp(entry->name, "src") == 0) return 0;
    if (strcmp(entry->name, "include") == 0) return 1;
    return 2;
}

static int compare_entries_core(const DirEntry* left, const DirEntry* right, bool isRoot) {
    if (left && right && left->type != right->type) {
        return (left->type == ENTRY_FOLDER) ? -1 : 1;
    }

    if (isRoot) {
        int lp = entry_priority_root(left);
        int rp = entry_priority_root(right);
        if (lp != rp) return (lp < rp) ? -1 : 1;
    }

    const char* ln = left && left->name ? left->name : "";
    const char* rn = right && right->name ? right->name : "";
    return strcasecmp(ln, rn);
}

#if defined(__APPLE__)
static int compare_entries_bsd(void* ctx, const void* a, const void* b) {
    const DirEntry* left = *(const DirEntry* const*)a;
    const DirEntry* right = *(const DirEntry* const*)b;
    bool isRoot = ctx != NULL;
    return compare_entries_core(left, right, isRoot);
}
#else
static int compare_entries_glibc(const void* a, const void* b, void* ctx) {
    const DirEntry* left = *(const DirEntry* const*)a;
    const DirEntry* right = *(const DirEntry* const*)b;
    bool isRoot = ctx != NULL;
    return compare_entries_core(left, right, isRoot);
}
#endif

static void sort_dir_entries(DirEntry* entry) {
    if (!entry || entry->childCount < 2) return;
    bool isRoot = (entry->parent == NULL);
#if defined(__APPLE__)
    qsort_r(entry->children, (size_t)entry->childCount, sizeof(DirEntry*),
            (void*)(isRoot ? entry : NULL), compare_entries_bsd);
#else
    qsort_r(entry->children, (size_t)entry->childCount, sizeof(DirEntry*),
            compare_entries_glibc, (void*)(isRoot ? entry : NULL));
#endif
}

static char* build_child_path(const char* parentPath, const char* childName) {
    if (!parentPath || !childName) return NULL;
    size_t parentLen = strlen(parentPath);
    size_t childLen = strlen(childName);
    size_t needsSlash = (parentLen > 0 && parentPath[parentLen - 1] == '/') ? 0u : 1u;
    size_t total = parentLen + needsSlash + childLen + 1u;
    char* out = (char*)malloc(total);
    if (!out) return NULL;

    memcpy(out, parentPath, parentLen);
    size_t pos = parentLen;
    if (needsSlash) out[pos++] = '/';
    memcpy(out + pos, childName, childLen);
    out[pos + childLen] = '\0';
    return out;
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
        fprintf(stderr, "[ProjectLoad] Failed to open directory: %s (errno=%d: %s)\n",
                path ? path : "(null)",
                errno,
                strerror(errno));
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
    root->isExpanded = (parent == NULL);

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char* fullPath = build_child_path(path, entry->d_name);
        if (!fullPath) {
            continue;
        }

        struct stat st;
        if (stat(fullPath, &st) == -1) {
            if (project_loader_debug_enabled()) {
                fprintf(stderr,
                        "[ProjectLoad] stat failed: %s (errno=%d: %s)\n",
                        fullPath,
                        errno,
                        strerror(errno));
            }
            free(fullPath);
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
        free(fullPath);
    }

    closedir(dir);
    sort_dir_entries(root);
    if (project_loader_debug_enabled()) {
        fprintf(stderr,
                "[ProjectLoad] path=%s type=%s children=%d\n",
                path ? path : "(null)",
                parent ? "child" : "root",
                root->childCount);
    }
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
