#include "rename_callbacks.h"
#include "app/GlobalInfo/project.h"
#include "core/Analysis/analysis_scheduler.h"
#include "core/InputManager/UserInput/rename_flow.h"
#include "ide/Panes/ToolPanels/Project/tool_project.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>  // for rename()

void handleProjectFileRenameCallback(const char* oldName, const char* newName, void* context) {
    (void)oldName;
    ProjectRenameContext* ctx = (ProjectRenameContext*)context;
    if (!ctx || !ctx->originalPath[0] || !newName || !newName[0]) return;
    if (strcmp(newName, ".") == 0 || strcmp(newName, "..") == 0) return;
    if (strchr(newName, '/')) return;

    const char* lastSlash = strrchr(ctx->originalPath, '/');
    if (!lastSlash) return;

    size_t dirLen = (size_t)(lastSlash - ctx->originalPath);

    char newPath[1024];
    if (snprintf(newPath, sizeof(newPath), "%.*s/%s", (int)dirLen, ctx->originalPath, newName) >= (int)sizeof(newPath)) {
        fprintf(stderr, "[Rename] New path too long.\n");
        return;
    }

    if (rename(ctx->originalPath, newPath) != 0) {
        fprintf(stderr, "[Rename] Failed to rename file: %s → %s (errno=%d: %s)\n",
                ctx->originalPath,
                newPath,
                errno,
                strerror(errno));
        return;
    }

    printf("[Rename] Renamed: %s → %s\n", ctx->originalPath, newPath);
    snprintf(ctx->originalPath, sizeof(ctx->originalPath), "%s", newPath);
    snprintf(ctx->originalName, sizeof(ctx->originalName), "%s", newName);

    queueProjectRefresh(ANALYSIS_REASON_PROJECT_MUTATION);
}

bool isRenameValid(const char* newName, void* context) {
    ProjectRenameContext* ctx = (ProjectRenameContext*)context;
    if (!ctx || !ctx->originalPath[0] || !newName || !newName[0]) return false;
    if (strcmp(newName, ".") == 0 || strcmp(newName, "..") == 0) return false;
    if (strchr(newName, '/')) return false;

    const char* lastSlash = strrchr(ctx->originalPath, '/');
    if (!lastSlash) return false;
    size_t dirLen = (size_t)(lastSlash - ctx->originalPath);

    char candidate[1024];
    if (snprintf(candidate, sizeof(candidate), "%.*s/%s", (int)dirLen, ctx->originalPath, newName) >= (int)sizeof(candidate)) {
        return false;
    }

    if (strcmp(candidate, ctx->originalPath) == 0) return true;
    return access(candidate, F_OK) != 0;
}
