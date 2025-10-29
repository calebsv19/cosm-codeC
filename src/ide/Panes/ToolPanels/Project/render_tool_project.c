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
extern int hoveredEntryDepth;


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

    // === Skip build folders entirely ===
    if (entry->type == ENTRY_FOLDER && strcmp(displayName, "BuildOutputs") == 0) return;

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

    SDL_Rect box;
    if (entry == renamingEntry) {
        char line[1024];
        snprintf(line, sizeof(line), "%s%s_", prefix, renameBuffer);
        int textWidth = getTextWidth(line);
        box = (SDL_Rect){ drawX - 6, drawY - 1, textWidth + 12, lineHeight };
        drawText(drawX, drawY, line);
    } else {
        char line[1024];
        snprintf(line, sizeof(line), "%s%s", prefix, displayName);
        int textWidth = getTextWidth(line);
        box = (SDL_Rect){ drawX - 6, drawY - 1, textWidth + 12, lineHeight };
        drawText(drawX, drawY, line);
    }

    if (mouseY >= drawY && mouseY < drawY + lineHeight) {
        hoveredEntry = entry;
        hoveredEntryDepth = depth;
        SDL_SetRenderDrawColor(renderer, 180, 180, 180, 100);
        SDL_RenderDrawRect(renderer, &box);
        hoveredEntryRect = box;
    }

    if (entry == selectedEntry) {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 180);
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
    int y = pane->y + 30;
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

    // --- [+] Add Folder ---
    projectBtnAddFolder = (SDL_Rect){ x, y, iconBtnSize, iconBtnSize };
    renderButton(projectBtnAddFolder, "+");
    drawText(x + iconBtnSize + 6, y + 4, "Add Folder");
    y += iconBtnSize + spacing;

    // --- [-] Delete Entry ---
    projectBtnDelete = (SDL_Rect){ x, y, iconBtnSize, iconBtnSize };
    renderButton(projectBtnDelete, "-");
    drawText(x + iconBtnSize + 6, y + 4, "Delete");
    y += iconBtnSize + spacing;

    // --- Render File Tree ---
    if (projectRoot) {
        renderTreeRecursive(projectRoot, x, &y, 0, maxY);
    } else {
        drawText(x, y, "(No project loaded)");
    }

}
