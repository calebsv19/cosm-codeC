#include "rename_callbacks.h"
#include "app/GlobalInfo/project.h"
#include "core/Analysis/analysis_scheduler.h"
#include "core/InputManager/UserInput/rename_flow.h"
#include "ide/Panes/ToolPanels/Project/tool_project.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>  // for rename()

void handleProjectFileRenameCallback(const char* oldName, const char* newName, void* context) {
    DirEntry* entry = (DirEntry*)context;
    if (!entry || !entry->path || !newName) return;

    const char* lastSlash = strrchr(entry->path, '/');
    if (!lastSlash) return;

    size_t dirLen = lastSlash - entry->path;

    char newPath[1024];
    snprintf(newPath, sizeof(newPath), "%.*s/%s", (int)dirLen, entry->path, newName);

    if (rename(entry->path, newPath) != 0) {
        fprintf(stderr, "[Rename] Failed to rename file: %s → %s\n", entry->path, newPath);
        return;
    }

    printf("[Rename] Renamed: %s → %s\n", entry->path, newPath);

    // Replace old path/name with newly allocated copies
    free(entry->path);
    entry->path = strdup(newPath);

    free(entry->name);
    entry->name = strdup(newName);

    queueProjectRefresh(ANALYSIS_REASON_PROJECT_MUTATION);
}

bool isRenameValid(const char* newName, DirEntry* entry) {
    if (!entry || !entry->parent || !newName || strlen(newName) == 0) return false;

    DirEntry* parent = entry->parent;

    for (int i = 0; i < parent->childCount; i++) {
        DirEntry* sibling = parent->children[i];
        if (!sibling || sibling == entry) continue;

        if (sibling->name && strcmp(sibling->name, newName) == 0) {
	    
            return false; // Conflict
        }
    }

    return true; // Unique
}
