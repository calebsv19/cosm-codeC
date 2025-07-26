#include "render_tool_libraries.h"

#include "engine/Render/render_pipeline.h"
#include "engine/Render/render_helpers.h"
#include "engine/Render/render_text_helpers.h"

#include "app/GlobalInfo/project.h"
#include "ide/Panes/ToolPanels/Libraries/tool_libraries.h"
#include <SDL2/SDL.h>


extern int libraryMouseX, libraryMouseY;
extern DirEntry* hoveredLibraryEntry;
extern DirEntry* selectedLibraryEntry;
extern int hoveredLibraryDepth;

static void renderLibraryTreeRecursive(DirEntry* entry, int x, int* y, int depth, int maxY) {
    if (*y > maxY) return;

    const char* name = strrchr(entry->path, '/');
    name = name ? name + 1 : entry->path;
    if (name[0] == '.' && strcmp(name, "..") != 0) return;

    RenderContext* ctx = getRenderContext();
    SDL_Renderer* renderer = ctx->renderer;
    int indent = depth * 20;
    int drawX = x + indent;
    int drawY = *y;
    int lineHeight = 22;

    const char* prefix = (entry->type == ENTRY_FOLDER)
        ? (entry->isExpanded ? "[-] " : "[+] ")
        : "    ";

    char line[1024];
    snprintf(line, sizeof(line), "%s%s", prefix, name);
    int textWidth = getTextWidth(line);
    SDL_Rect box = { drawX - 6, drawY - 1, textWidth + 12, lineHeight };

    if (libraryMouseY >= drawY && libraryMouseY < drawY + lineHeight) {
        hoveredLibraryEntry = entry;
        hoveredLibraryDepth = depth;
        SDL_SetRenderDrawColor(renderer, 180, 180, 180, 100);
        SDL_RenderDrawRect(renderer, &box);
    }

    if (entry == selectedLibraryEntry) {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 180);
        SDL_RenderDrawRect(renderer, &box);
    }

    drawText(drawX, drawY, line);
    *y += lineHeight;

    if (entry->type == ENTRY_FOLDER && entry->isExpanded) {
        for (int i = 0; i < entry->childCount; i++) {
            renderLibraryTreeRecursive(entry->children[i], x, y, depth + 1, maxY);
        }
    }
}




void renderLibrariesPanel(UIPane* pane) {
    int x = pane->x + 12;
    int y = pane->y + 32;
    int maxY = pane->y + pane->h;
    hoveredLibraryEntry = NULL;

    if (libraryRoot) {
        renderLibraryTreeRecursive(libraryRoot, x, &y, 0, maxY);
    } else {
        drawText(x, y, "(No libraries found)");
    }
}


