#include "Render/ToolPanels/render_tool_project.h"
#include "Render/render_pipeline.h"      // drawText, getTextWidth, getRenderContext
#include "Render/render_helpers.h"
#include "Render/render_text_helpers.h"

#include "GlobalInfo/project.h"          // projectRoot

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
    if (displayName[0] == '.' && strcmp(displayName, "..") != 0) return;

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
    snprintf(line, sizeof(line), "%s%s", prefix, displayName);
    int textWidth = getTextWidth(line);
    SDL_Rect box = { drawX - 6, drawY - 1, textWidth + 12, lineHeight };

    if (mouseY >= drawY && mouseY < drawY + lineHeight) {
        hoveredEntry = entry;
        hoveredEntryDepth = depth;
        SDL_SetRenderDrawColor(renderer, 180, 180, 180, 100);
        SDL_RenderDrawRect(renderer, &box);
    }

    if (entry == selectedEntry) {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 180);
        SDL_RenderDrawRect(renderer, &box);
    }

    drawText(drawX, drawY, line);
    *y += lineHeight;

    if (entry->type == ENTRY_FOLDER && entry->isExpanded) {
        for (int i = 0; i < entry->childCount; i++) {
            renderTreeRecursive(entry->children[i], x, y, depth + 1, maxY);
        }
    }
}

void renderProjectFilesPanel(UIPane* pane) {
    int x = pane->x + 12;
    int y = pane->y + 32;
    int maxY = pane->y + pane->h;
    hoveredEntry = NULL;

    if (projectRoot) {
        renderTreeRecursive(projectRoot, x, &y, 0, maxY);
    } else {
        drawText(x, y, "(No project loaded)");
    }
}

