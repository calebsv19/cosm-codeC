#include "render_terminal.h"
#include "render_helpers.h"
#include "render_text_helpers.h"  // renderUIPane, drawText
#include "../GlobalInfo/system_control.h"

#include "../Terminal/terminal.h"
#include "../pane.h"

#include <SDL2/SDL.h>

void renderTerminalContents(UIPane* pane, bool hovered, struct IDECoreState* core) {

/*
    RenderContext* ctx = getRenderContext();
    if (!ctx || !ctx->renderer) return;
    SDL_Renderer* renderer = ctx->renderer;
*/

    renderUIPane(pane, hovered);

    int lineHeight = 20;
    int padding = 6;
    int visibleLines = (pane->h - 2 * padding) / lineHeight;

    int totalLines = getTerminalLineCount();
    const char** lines = getTerminalBuffer();

    int start = (totalLines > visibleLines)
        ? totalLines - visibleLines
        : 0;

    for (int i = 0; i < visibleLines && (start + i) < totalLines; i++) {
        drawClippedText(pane->x + padding,
                        pane->y + padding + i * lineHeight,
                        lines[start + i],
                        pane->w - 2 * padding);
    }
}

