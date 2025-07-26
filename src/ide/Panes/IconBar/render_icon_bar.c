
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

    drawText(pane->x + 8, pane->y + 6, pane->title);

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
    }
}

