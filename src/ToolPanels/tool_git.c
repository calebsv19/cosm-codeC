#include "tool_git.h"

#include "Render/render_pipeline.h"
#include "GlobalInfo/project.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

GitFileEntry gitFiles[MAX_GIT_ENTRIES];
int gitFileCount = 0;
char currentGitBranch[64] = "unknown";

void refreshGitStatus() {
    gitFileCount = 0;

    // Get branch name
    char branchCmd[1024];
    snprintf(branchCmd, sizeof(branchCmd), "cd %s && git branch --show-current", projectPath);
    FILE* bpipe = popen(branchCmd, "r");
    if (bpipe) {
        fgets(currentGitBranch, sizeof(currentGitBranch), bpipe);
        currentGitBranch[strcspn(currentGitBranch, "\n")] = 0;
        pclose(bpipe);
    }

    // Get file status
    char statusCmd[1024];
    snprintf(statusCmd, sizeof(statusCmd), "cd %s && git status --porcelain", projectPath);
    FILE* pipe = popen(statusCmd, "r");
    if (!pipe) return;

    char line[512];
    while (fgets(line, sizeof(line), pipe) && gitFileCount < MAX_GIT_ENTRIES) {
        if (strlen(line) < 4) continue;
        GitFileEntry* f = &gitFiles[gitFileCount++];

        char statusCode[3];
        strncpy(statusCode, line, 2);
        statusCode[2] = '\0';
        strcpy(f->path, line + 3);
        f->path[strcspn(f->path, "\n")] = 0;

        if (statusCode[0] == 'M') {
            f->status = GIT_STATUS_STAGED;
            f->staged = true;
        } else if (statusCode[1] == 'M') {
            f->status = GIT_STATUS_MODIFIED;
            f->staged = false;
        } else if (statusCode[1] == 'A') {
            f->status = GIT_STATUS_ADDED;
            f->staged = false;
        } else if (statusCode[1] == 'D') {
            f->status = GIT_STATUS_DELETED;
            f->staged = false;
        } else if (statusCode[1] == '?') {
            f->status = GIT_STATUS_UNTRACKED;
            f->staged = false;
        }
    }

    pclose(pipe);
}


/*
void handleGitPanelEvent(UIPane* pane, SDL_Event* event) {
    if (event->type != SDL_MOUSEBUTTONDOWN || event->button.button != SDL_BUTTON_LEFT) return;

    int mx = event->button.x;
    int my = event->button.y;

    int x = pane->x + 12;
    int y = pane->y + 32;
    int lineHeight = 20;

    // Skip over branch label and section headers
    y += lineHeight * 2;  // branch label spacing
    y += lineHeight;      // [Changes] label

    for (int i = 0; i < gitFileCount; i++) {
        SDL_Rect rect = {x + 20, y, pane->w - 24, lineHeight};
        if (my >= rect.y && my <= rect.y + rect.h &&
            mx >= rect.x && mx <= rect.x + rect.w) {

            printf("Clicked file: %s (status=%d)\n", gitFiles[i].path, gitFiles[i].status);

            // (Future) Toggle staging here
            break;
        }
        y += lineHeight;
    }
}
*/
