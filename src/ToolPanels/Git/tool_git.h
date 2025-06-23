#ifndef TOOL_GIT_H
#define TOOL_GIT_H

#include "PaneInfo/pane.h"
#include <SDL2/SDL.h>

typedef enum {
    GIT_STATUS_MODIFIED,
    GIT_STATUS_ADDED,
    GIT_STATUS_DELETED,
    GIT_STATUS_UNTRACKED,
    GIT_STATUS_STAGED
} GitStatus;

typedef struct {
    char path[256];       // e.g. "src/main.c"
    GitStatus status;
    bool staged;
} GitFileEntry;

#define MAX_GIT_ENTRIES 512

extern GitFileEntry gitFiles[MAX_GIT_ENTRIES];
extern int gitFileCount;
extern char currentGitBranch[64];

void refreshGitStatus(); // Will run `git status` and populate the list

void handleGitPanelEvent(UIPane* pane, SDL_Event* event);

#endif

