#include "ide/Panes/ToolPanels/Project/render_tool_project.h"
#include "ide/Panes/ToolPanels/Project/tool_project.h"
#include "engine/Render/render_pipeline.h"      // drawText, getTextWidth, getRenderContext
#include "engine/Render/render_helpers.h"
#include "engine/Render/render_text_helpers.h"
#include "app/GlobalInfo/core_state.h"

#include "app/GlobalInfo/project.h"          // projectRoot

#include <SDL2/SDL.h>
#include <string.h>                      // strrchr
#include <stdio.h>                       // snprintf

// Internal static state shared with logic file
extern int mouseX, mouseY;               // from tool_project.c
extern DirEntry* hoveredEntry;
extern DirEntry* selectedEntry;
extern DirEntry* selectedFile;
extern DirEntry* selectedDirectory;
extern int hoveredEntryDepth;
extern char runTargetPath[1024];


static void renderTreeRecursive(DirEntry* entry, int x, int* y, int depth, int maxY) {
    if (*y > maxY) return;

    const char* displayName = strrchr(entry->path, '/');
    displayName = (displayName) ? displayName + 1 : entry->path;

    // Skip hidden files except for ".."
    if (displayName[0] == '.' && strcmp(displayName, "..") != 0) return;

    // === Skip Build Artifacts ===
    if (entry->type == ENTRY_FILE) {
        const char* ext = strrchr(displayName, '.');
        if (ext && (
            strcmp(ext, ".o") == 0 ||
            strcmp(ext, ".obj") == 0 ||
            strcmp(ext, ".out") == 0
        )) return;

        if (strcmp(displayName, "last_build") == 0) return;
    }

    RenderContext* ctx = getRenderContext();
    if (!ctx || !ctx->renderer) return;
    SDL_Renderer* renderer = ctx->renderer;

    int indent = depth * 20;
    int drawX = x + indent;
    int drawY = *y;
    int lineHeight = 22;

    const char* prefix = "";
    if (entry->type == ENTRY_FOLDER) {
        prefix = entry->isExpanded ? "[-] " : "[+] ";
    }

    char line[1024];
    if (entry == renamingEntry) {
        snprintf(line, sizeof(line), "%s%s_", prefix, renameBuffer);
    } else {
        snprintf(line, sizeof(line), "%s%s", prefix, displayName);
    }

    int textWidth = getTextWidth(line);
    SDL_Rect box = (SDL_Rect){ drawX - 6, drawY - 1, textWidth + 12, lineHeight };

    if (entry == selectedDirectory) {
        SDL_SetRenderDrawColor(renderer, 80, 160, 90, 120);
        SDL_RenderFillRect(renderer, &box);
    }

    if (entry == selectedFile) {
        SDL_SetRenderDrawColor(renderer, 70, 120, 200, 140);
        SDL_RenderFillRect(renderer, &box);
    }

    const char* workspace = getWorkspacePath();
    bool entryInBuildOutputs = false;
    if (workspace && workspace[0]) {
        size_t workspaceLen = strlen(workspace);
        if (strncmp(entry->path, workspace, workspaceLen) == 0) {
            const char* relative = entry->path + workspaceLen;
            if (*relative == '/' || *relative == '\\') relative++;
            if (strncmp(relative, "BuildOutputs", strlen("BuildOutputs")) == 0) {
                entryInBuildOutputs = true;
            }
        }
    }

    bool isRunTarget = (entryInBuildOutputs && runTargetPath[0] != '\0' && strcmp(entry->path, runTargetPath) == 0);
    bool isRunAncestor = false;
    if (!isRunTarget && entryInBuildOutputs && runTargetPath[0] != '\0') {
        size_t len = strlen(entry->path);
        if (strncmp(runTargetPath, entry->path, len) == 0) {
            char next = runTargetPath[len];
            if (next == '/' || next == '\\') {
                isRunAncestor = true;
            }
        }
    }

    if (isRunTarget) {
        SDL_SetRenderDrawColor(renderer, 200, 60, 60, 120);
        SDL_RenderFillRect(renderer, &box);
    } else if (isRunAncestor) {
        SDL_SetRenderDrawColor(renderer, 200, 60, 60, 80);
        SDL_RenderDrawRect(renderer, &box);
    }

    drawText(drawX, drawY, line);

    if (mouseY >= drawY && mouseY < drawY + lineHeight) {
        hoveredEntry = entry;
        hoveredEntryDepth = depth;
        SDL_SetRenderDrawColor(renderer, 180, 180, 180, 100);
        SDL_RenderDrawRect(renderer, &box);
        hoveredEntryRect = box;
    }

    if (entry == selectedEntry) {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 200);
        SDL_RenderDrawRect(renderer, &box);
    }

    *y += lineHeight;

    if (entry->type == ENTRY_FOLDER && entry->isExpanded) {
        for (int i = 0; i < entry->childCount; i++) {
            renderTreeRecursive(entry->children[i], x, y, depth + 1, maxY);
        }
    }
}



void renderProjectFilesPanel(UIPane* pane) {
    int x = pane->x + 12;
    const char* workspace = getWorkspacePath();
    char workspaceLabel[512];
    if (workspace && workspace[0]) {
        snprintf(workspaceLabel, sizeof(workspaceLabel), "Workspace: %s", workspace);
    } else {
        snprintf(workspaceLabel, sizeof(workspaceLabel), "Workspace: (sample project)");
    }

    drawClippedText(x, pane->y + 8, workspaceLabel, pane->w - 24);

    const char* runTarget = getRunTargetPath();
    char runLabel[512];
    if (runTarget && runTarget[0]) {
        snprintf(runLabel, sizeof(runLabel), "Run target: %s", runTarget);
    } else {
        snprintf(runLabel, sizeof(runLabel), "Run target: (auto-select on build)");
    }
    drawClippedText(x, pane->y + 22, runLabel, pane->w - 24);

    int y = pane->y + 42;
    int maxY = pane->y + pane->h;
    hoveredEntry = NULL;
    hoveredEntryRect = (SDL_Rect){0};

    const int iconBtnSize = 24;
    const int spacing = 8;

    // --- [+] Add File ---
    projectBtnAddFile = (SDL_Rect){ x, y, iconBtnSize, iconBtnSize };
    renderButton(projectBtnAddFile, "+");
    drawText(x + iconBtnSize + 6, y + 4, "Add File");
    y += iconBtnSize + spacing;

    // --- [-] Delete File ---
    projectBtnDeleteFile = (SDL_Rect){ x, y, iconBtnSize, iconBtnSize };
    renderButton(projectBtnDeleteFile, "-");
    drawText(x + iconBtnSize + 6, y + 4, "Delete File");
    y += iconBtnSize + spacing;

    // --- [+] Add Folder ---
    projectBtnAddFolder = (SDL_Rect){ x, y, iconBtnSize, iconBtnSize };
    renderButton(projectBtnAddFolder, "+");
    drawText(x + iconBtnSize + 6, y + 4, "Add Folder");
    y += iconBtnSize + spacing;

    // --- [-] Delete Folder ---
    projectBtnDeleteFolder = (SDL_Rect){ x, y, iconBtnSize, iconBtnSize };
    renderButton(projectBtnDeleteFolder, "-");
    drawText(x + iconBtnSize + 6, y + 4, "Delete Folder");
    y += iconBtnSize + spacing;

    // --- Render File Tree ---
    if (projectRoot) {
        renderTreeRecursive(projectRoot, x, &y, 0, maxY);
    } else {
        drawText(x, y, "(No project loaded)");
    }

}
