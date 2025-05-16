#include "render_control_panel.h"
#include "render_pipeline.h"  // renderUIPane, drawText
#include "render_helpers.h"

#include "../ControlPanel/control_panel.h"

#include "../GlobalInfo/system_control.h"

#include "../pane.h"

#include <SDL2/SDL.h>

void renderControlPanelContents(UIPane* pane, bool hovered, struct IDECoreState* core) {
    RenderContext* ctx = getRenderContext();
    if (!ctx || !ctx->renderer) return;
    SDL_Renderer* renderer = ctx->renderer;

    renderUIPane(pane, hovered);

    int padding = 8;
    int x = pane->x + padding;
    int y = pane->y + padding;

    // Panel title
    drawText(x, y, pane->title);
    y += 28;

    // Search bar label
    drawText(x, y, "Search:");
    y += 20;

    // Draw mock search box
    SDL_Rect searchBox = { x, y, pane->w - 2 * padding, 24 };
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 20);  // faint fill
    SDL_RenderFillRect(renderer, &searchBox);
    SDL_SetRenderDrawColor(renderer, 180, 180, 180, 255);
    SDL_RenderDrawRect(renderer, &searchBox);
    drawText(x + 6, y + 4, "type to filter...");
    y += 36;

    // Section label
    drawText(x, y, "Options:");
    y += 24;

    // Toggle 1
    drawText(x + 12, y, isLiveParseEnabled() ? "[x] Live Parse" : "[ ] Live Parse");
    y += 20;
 
    // Toggle 2
    drawText(x + 12, y, isShowInlineErrorsEnabled() ? "[x] Show Errors Inline" : "[ ] Show Errors Inline");
    y += 20;

    // Placeholder for more controls
    drawText(x, y + 12, "...more controls soon");
}

