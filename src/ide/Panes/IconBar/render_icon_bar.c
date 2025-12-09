
#include "ide/Panes/IconBar/render_icon_bar.h"
#include "engine/Render/render_pipeline.h"            
#include "engine/Render/render_helpers.h"    // for drawText and renderUIPane
#include "engine/Render/render_text_helpers.h"

#include "app/GlobalInfo/system_control.h"


#include "ide/Panes/PaneInfo/pane.h"
#include "ide/Panes/IconBar/icon_bar.h"   // for getActiveIcon() and ICON_COUNT

#include <SDL2/SDL.h>

void renderIconBarContents(UIPane* pane, bool hovered, struct IDECoreState* core) {
    RenderContext* ctx = getRenderContext();
    if (!ctx || !ctx->renderer) return;
    SDL_Renderer* renderer = ctx->renderer;

    SelectableTextOptions titleOpts = {
        .pane = pane,
        .owner = pane,
        .owner_role = pane->role,
        .x = pane->x + 8,
        .y = pane->y + 6,
        .maxWidth = pane->w - 16,
        .text = pane->title,
        .flags = TEXT_SELECTION_FLAG_SELECTABLE,
    };
    drawSelectableText(&titleOpts);

    int iconCount = ICON_COUNT;
    int iconSize = pane->w - 20;
    int spacing = 12;
    int startY = pane->y + 28;

    int centerX = pane->x + pane->w / 2;
    int iconX = centerX - iconSize / 2;

    IconTool active = getActiveIcon();

    for (int i = 0; i < iconCount; i++) {
        int iconY = startY + i * (iconSize + spacing);

        SDL_Rect icon = {
            .x = iconX,
            .y = iconY,
            .w = iconSize,
            .h = iconSize
        };

        // Fill
        if (i == active) {
            SDL_SetRenderDrawColor(renderer, 140, 140, 140, 255); // Highlight
        } else {
            SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255); // Normal
        }
        SDL_RenderFillRect(renderer, &icon);

        // Border
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDrawRect(renderer, &icon);

        // Letter label per icon
        const char* label = "?";
        switch (i) {
            case ICON_PROJECT_FILES: label = "P"; break;
            case ICON_LIBRARIES:     label = "L"; break;
            case ICON_BUILD_OUTPUT:  label = "B"; break;
            case ICON_ERRORS:        label = "E"; break;
            case ICON_ASSET_MANAGER:   label = "A"; break;
            case ICON_TASKS:           label = "T"; break;
            case ICON_VERSION_CONTROL: label = "G"; break; // Git
            default:                 label = "";  break;
        }
        if (label && label[0]) {
            int textW = getTextWidth(label);
            int textH = 16;
            int tx = icon.x + (icon.w - textW) / 2;
            int ty = icon.y + (icon.h - textH) / 2;
            drawText(tx, ty, label);
        }
    }
}
