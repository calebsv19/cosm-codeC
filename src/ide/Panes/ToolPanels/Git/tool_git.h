#ifndef TOOL_GIT_H
#define TOOL_GIT_H

#include "ide/Panes/PaneInfo/pane.h"
#include <SDL2/SDL.h>
#include <stdbool.h>

typedef enum {
    GIT_STATUS_MODIFIED,
    GIT_STATUS_ADDED,
    GIT_STATUS_DELETED,
    GIT_STATUS_UNTRACKED,
    GIT_STATUS_STAGED
} GitStatus;

typedef struct {
    char path[256];       // Relative file path, e.g. "src/main.c"
    GitStatus status;     // Git status enum
    bool staged;          // Explicit staging flag
} GitFileEntry;

#define MAX_GIT_ENTRIES 512

extern GitFileEntry gitFiles[MAX_GIT_ENTRIES];
extern int gitFileCount;
extern char currentGitBranch[64];

// Run git status and branch to populate current git state
void refreshGitStatus(void);

#endif // TOOL_GIT_H

