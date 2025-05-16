#include "Render/ToolPanels/render_tool_assets.h"
#include "Render/render_helpers.h"

#include "GlobalInfo/project.h"    // for DirEntry
#include "ToolPanels/tool_assets.h"

#include <SDL2/SDL.h>
#include <string.h>

// === FUTURE: store selectedAssetEntry and hoveredAssetEntry ===

static void renderAssetList(DirEntry* entry, int x, int* y, int maxY) {
    if (!entry || entry->type != ENTRY_FOLDER) return;

    int lineHeight = 20;
    for (int i = 0; i < entry->childCount; i++) {
        DirEntry* file = entry->children[i];
        if (file->type != ENTRY_FILE) continue;

        const char* name = strrchr(file->path, '/');
        name = name ? name + 1 : file->path;

        if (*y + lineHeight > maxY) break;

        drawText(x, *y, name);
        *y += lineHeight;

        // FUTURE: add hover logic or preview tooltip if hovered
    }
}


void renderAssetManagerPanel(UIPane* pane) {
    int x = pane->x + 12;
    int y = pane->y + 32;
    int maxY = pane->y + pane->h;

    if (assetRoot) {
        renderAssetList(assetRoot, x, &y, maxY);
    } else {
        drawText(x, y, "(No assets loaded)");
    }

    // FUTURE: add preview image/audio/info window based on selection
}

