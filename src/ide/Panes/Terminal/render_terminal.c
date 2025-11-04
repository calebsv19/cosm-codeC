#include "ide/Panes/Terminal/render_terminal.h"
#include "engine/Render/render_helpers.h"
#include "engine/Render/render_text_helpers.h"  // renderUIPane, drawText
#include "app/GlobalInfo/system_control.h"

#include "../Terminal/terminal.h"
#include "core/TextSelection/text_selection_manager.h"
#include "ide/Panes/PaneInfo/pane.h"

#include <SDL2/SDL.h>
#include <string.h>

void renderTerminalContents(UIPane* pane, bool hovered, struct IDECoreState* core) {

    RenderContext* ctx = getRenderContext();
    if (!ctx || !ctx->renderer) return;
    SDL_Renderer* renderer = ctx->renderer;

    renderUIPane(pane, hovered);

    int lineHeight = TERMINAL_LINE_HEIGHT;
    int padding = TERMINAL_PADDING;
    int visibleLines = (pane->h - 2 * padding) / lineHeight;

    int totalLines = getTerminalLineCount();
    const char** lines = getTerminalBuffer();

    int start = (totalLines > visibleLines)
        ? totalLines - visibleLines
        : 0;

    int selStartLine = 0, selStartCol = 0, selEndLine = 0, selEndCol = 0;
    bool hasSelection = terminal_get_selection_bounds(&selStartLine, &selStartCol, &selEndLine, &selEndCol);

    for (int i = 0; i < visibleLines && (start + i) < totalLines; i++) {
        int lineIndex = start + i;
        const char* text = lines[lineIndex];
        int drawX = pane->x + padding;
        int drawY = pane->y + padding + i * lineHeight;
        int maxWidth = pane->w - 2 * padding;

        if (hasSelection && lineIndex >= selStartLine && lineIndex <= selEndLine) {
            int lineLen = text ? (int)strlen(text) : 0;
            int startCol = (lineIndex == selStartLine) ? selStartCol : 0;
            int endCol = (lineIndex == selEndLine) ? selEndCol : lineLen;
            if (startCol < 0) startCol = 0;
            if (endCol < startCol) endCol = startCol;
            if (endCol > lineLen) endCol = lineLen;

            if (startCol != endCol) {
                int startX = drawX + getTextWidthN(text, startCol);
                int endX = drawX + getTextWidthN(text, endCol);
                int width = endX - startX;
                if (width <= 0) width = 2;
                SDL_Rect highlight = { startX, drawY, width, lineHeight };
                SDL_SetRenderDrawColor(renderer, 80, 120, 200, 100);
                SDL_RenderFillRect(renderer, &highlight);
            }
        }

        drawClippedText(drawX, drawY, text, maxWidth);

        int lineWidth = getTextWidth(text);
        if (lineWidth <= 0) lineWidth = maxWidth;
        int lineLen = text ? (int)strlen(text) : 0;
        TextSelectionRect rect = {
            .bounds = { drawX, drawY, lineWidth, lineHeight },
            .line = lineIndex,
            .column_start = 0,
            .column_end = lineLen,
        };
        TextSelectionDescriptor desc = {
            .owner = pane,
            .owner_role = pane->role,
            .text = text,
            .text_length = (size_t)lineLen,
            .rects = &rect,
            .rect_count = 1,
            .flags = TEXT_SELECTION_FLAG_SELECTABLE,
            .copy_handler = NULL,
            .copy_user_data = NULL,
        };
        text_selection_manager_register(&desc);
    }
}
