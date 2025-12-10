#include "tool_git.h"

#include "engine/Render/render_pipeline.h"
#include "app/GlobalInfo/project.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

GitFileEntry gitFiles[MAX_GIT_ENTRIES];
int gitFileCount = 0;
char currentGitBranch[64] = "unknown";
GitLogEntry gitLogEntries[MAX_GIT_LOG_ENTRIES];
int gitLogCount = 0;

void refreshGitStatus() {
    gitFileCount = 0;

    // Get branch name
    char branchCmd[1024];
    snprintf(branchCmd, sizeof(branchCmd), "cd %s && git branch --show-current", projectPath);
    printf("[Git] Using projectPath: %s\n", projectPath);

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
    if (!pipe) {
        fprintf(stderr, "[GitError] Failed to run git status\n");
        return;
    }

    char line[512];
    while (fgets(line, sizeof(line), pipe) && gitFileCount < MAX_GIT_ENTRIES) {
        if (strlen(line) < 4) continue;

        GitFileEntry* f = &gitFiles[gitFileCount++];

        char statusCode[3];
        strncpy(statusCode, line, 2);
        statusCode[2] = '\0';

        char* filePath = line + 3;
        filePath[strcspn(filePath, "\n")] = 0;

        strncpy(f->path, filePath, sizeof(f->path) - 1);
        f->path[sizeof(f->path) - 1] = '\0';

        printf("[GitDebug] Raw line: %s", line);
        printf("[GitDebug] Parsed status: '%s', file: '%s'\n", statusCode, f->path);

        // Parse status into enum
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
        } else {
            f->status = GIT_STATUS_MODIFIED;  // Fallback
            f->staged = false;
        }
    }

    printf("[GitDebug] Final file count: %d\n", gitFileCount);
    pclose(pipe);
}

void refreshGitLog(int maxEntries) {
    gitLogCount = 0;
    if (maxEntries <= 0 || maxEntries > MAX_GIT_LOG_ENTRIES) {
        maxEntries = MAX_GIT_LOG_ENTRIES;
    }

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "cd %s && git log --oneline -n %d", projectPath, maxEntries);
    FILE* pipe = popen(cmd, "r");
    if (!pipe) {
        fprintf(stderr, "[GitError] Failed to run git log\n");
        return;
    }

    char line[GIT_LOG_LINE_MAX];
    while (fgets(line, sizeof(line), pipe) && gitLogCount < maxEntries) {
        // Expected format: "<hash> <subject...>"
        char* nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        if (line[0] == '\0') continue;

        char* space = strchr(line, ' ');
        if (!space) continue;
        *space = '\0';

        GitLogEntry* e = &gitLogEntries[gitLogCount++];
        strncpy(e->hash, line, sizeof(e->hash) - 1);
        e->hash[sizeof(e->hash) - 1] = '\0';

        snprintf(e->message, sizeof(e->message), "%s", space + 1);
    }

    pclose(pipe);
}
