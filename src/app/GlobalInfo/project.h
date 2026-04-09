#ifndef PROJECT_H
#define PROJECT_H

#include <stdbool.h>
#include <SDL2/SDL.h>




// --- Directory Tree System ---

typedef enum {
    ENTRY_FILE,
    ENTRY_FOLDER
} EntryType;

typedef struct DirEntry {
    EntryType type;
    char* name;             // Just the filename or folder name
    char* path;             // Full absolute path


    struct DirEntry* parent; // NEW: Pointer to parent DirEntry
    struct DirEntry** children;

    int childCount;
    int childCapacity;

    bool isExpanded; // whether this folder is open or collapsed
} DirEntry;

// --- Global Project State ---

struct UIPane;
extern DirEntry* projectRoot;       // Root of currently loaded project (NULL if none)
extern char projectPath[1024];       // Full path to the project root directory
extern char projectRootPath[1024];   // Full path to IDE root
extern bool pendingProjectRefresh;
extern unsigned int pendingProjectRefreshReasonMask;

void queueProjectRefresh(unsigned int analysisReasonMask);


void initProjectPaths(void);
void ide_apply_workspace_root_input(const char* path, bool persistPreference);

// --- Project Management API ---


DirEntry* scanDirectory(const char* path, int depth);

void freeDirectory(DirEntry* root);

// Load a directory recursively into memory
DirEntry* loadProjectDirectory(const char* path);

// Free an entire loaded directory tree
void destroyProjectDirectory(DirEntry* entry);

// Render the project tree (text-based for now)
void renderProjectTree(struct UIPane* pane, SDL_Renderer* renderer, DirEntry* root);

// Handle clicks inside project tree view
void handleProjectTreeClick(int mouseX, int mouseY);



#endif // PROJECT_H
