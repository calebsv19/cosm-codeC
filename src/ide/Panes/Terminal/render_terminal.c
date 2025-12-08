#include "ide/Panes/Terminal/render_terminal.h"
#include "engine/Render/render_helpers.h"
#include "engine/Render/render_text_helpers.h"  // renderUIPane, drawText
#include "app/GlobalInfo/system_control.h"
#include "app/GlobalInfo/core_state.h"

#include "../Terminal/terminal.h"
#include "core/TextSelection/text_selection_manager.h"
#include "ide/Panes/Terminal/terminal_grid.h"
#include "ide/Panes/Terminal/terminal.h" // for globals exported from terminal.c
#include "ide/UI/scroll_manager.h"
#include "ide/Panes/PaneInfo/pane.h"

#include <SDL2/SDL.h>
#include <string.h>

void renderTerminalContents(UIPane* pane, bool hovered, struct IDECoreState* core) {

    RenderContext* ctx = getRenderContext();
    if (!ctx || !ctx->renderer) return;
    SDL_Renderer* renderer = ctx->renderer;

    renderUIPane(pane, hovered);

    const int padding = TERMINAL_PADDING;
    const int trackWidth = 6;
    const int trackPadding = 4;

    SDL_Rect viewport = {
        .x = pane->x + padding,
        .y = pane->y + padding,
        .w = pane->w - (padding * 2 + trackWidth + trackPadding),
        .h = pane->h - (padding * 2)
    };
    if (viewport.w < 0) viewport.w = 0;
    if (viewport.h < 0) viewport.h = 0;

    int totalLines = getTerminalLineCount();
    (void)totalLines;
    terminal_resize_grid_for_pane(viewport.w, viewport.h);

    PaneScrollState* scroll = terminal_get_scroll_state();
    scroll_state_set_viewport(scroll, (float)viewport.h);
    // Use grid height for scrolling
    float contentHeight = (float)g_cellHeight * (float)g_termGrid.rows;
    scroll_state_set_content_height(scroll, contentHeight);

    if (terminal_is_following_output()) {
        float maxOffset = contentHeight - scroll->viewport_height_px;
        if (maxOffset < 0.0f) maxOffset = 0.0f;
        scroll->target_offset_px = maxOffset;
        scroll->offset_px = maxOffset;
    } else {
        scroll_state_clamp(scroll);
    }

    float offset = scroll_state_get_offset(scroll);
    int firstRow = (g_cellHeight > 0) ? (int)(offset / (float)g_cellHeight) : 0;
    if (firstRow < 0) firstRow = 0;
    if (firstRow > g_termGrid.rows) firstRow = g_termGrid.rows;
    float intraLineOffset = offset - (float)firstRow * (float)g_cellHeight;

    pushClipRect(&viewport);

    int rowsToRender = (viewport.h > 0) ? (viewport.h / g_cellHeight) + 1 : g_termGrid.rows;
    int cols = g_termGrid.cols;
    if (!g_termGrid.cells || g_termGrid.rows <= 0 || g_termGrid.cols <= 0) {
        popClipRect();
        return;
    }
    char* lineBuf = (char*)malloc((size_t)cols + 1);
    if (lineBuf) {
        for (int r = 0; r < rowsToRender; ++r) {
            int rowIndex = firstRow + r;
            if (rowIndex >= g_termGrid.rows) break;

            float drawYf = (float)viewport.y + (float)r * (float)g_cellHeight - intraLineOffset;
            if (drawYf >= (float)(viewport.y + viewport.h)) break;
            if (drawYf + g_cellHeight <= viewport.y) continue;

            int drawY = (int)drawYf;
            int lastNonSpace = -1;
            for (int c = 0; c < cols; ++c) {
                TermCell* cell = term_grid_cell(&g_termGrid, rowIndex, c);
                char ch = cell ? (char)(cell->ch ? cell->ch : ' ') : ' ';
                if (ch >= 0x20 && ch < 0x7F) {
                    lineBuf[c] = ch;
                    if (ch != ' ') lastNonSpace = c;
                } else {
                    lineBuf[c] = ' ';
                }
            }
            int renderLen = (lastNonSpace >= 0) ? lastNonSpace + 1 : 0;
            lineBuf[renderLen] = '\0';
            if (renderLen > 0) {
                drawText(viewport.x, drawY, lineBuf);
            }
        }
        free(lineBuf);
    }

    popClipRect();

    bool paneHovered = core && core->activeMousePane == pane;
    bool paneActive = core && core->focusedPane == pane;
    bool showScrollbar = scroll_state_can_scroll(scroll) && viewport.h > 0 &&
                         (paneHovered || paneActive || scroll_state_is_dragging_thumb(scroll));

    if (showScrollbar) {
        SDL_Rect track = {
            viewport.x + viewport.w + trackPadding,
            viewport.y,
            trackWidth,
            viewport.h
        };
        SDL_Rect thumb = scroll_state_thumb_rect(scroll, track.x, track.y, track.w, track.h);
        SDL_Color trackColor = scroll->track_color;
        SDL_Color thumbColor = scroll->thumb_color;
        SDL_SetRenderDrawColor(renderer, trackColor.r, trackColor.g, trackColor.b, trackColor.a);
        SDL_RenderFillRect(renderer, &track);
        SDL_SetRenderDrawColor(renderer, thumbColor.r, thumbColor.g, thumbColor.b, thumbColor.a);
        SDL_RenderFillRect(renderer, &thumb);
        terminal_set_scroll_track(&track, &thumb);
    } else {
        terminal_set_scroll_track(NULL, NULL);
    }

    if (paneActive) {
        // Caret at grid cursor position
        int caretRow = g_termGrid.cursor_row;
        int caretCol = g_termGrid.cursor_col;
        int caretX = viewport.x + caretCol * g_cellWidth;
        int caretY = viewport.y + (caretRow - firstRow) * g_cellHeight - (int)intraLineOffset;
        SDL_Rect caret = { caretX, caretY, g_cellWidth > 0 ? g_cellWidth : 8, g_cellHeight > 0 ? g_cellHeight : 3 };
        SDL_SetRenderDrawColor(renderer, 200, 200, 220, 200);
        SDL_RenderFillRect(renderer, &caret);
    }
}
