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
    const int headerH = TERMINAL_HEADER_HEIGHT;

    SDL_Rect header = {
        .x = pane->x + padding,
        .y = pane->y + padding,
        .w = pane->w - padding * 2,
        .h = headerH
    };

    SDL_Rect viewport = {
        .x = pane->x + padding,
        .y = pane->y + padding + headerH,
        .w = pane->w - (padding * 2 + trackWidth + trackPadding),
        .h = pane->h - (padding * 2 + headerH)
    };
    if (viewport.w < 0) viewport.w = 0;
    if (viewport.h < 0) viewport.h = 0;

    // Draw header tabs
    SDL_SetRenderDrawColor(renderer, 40, 40, 50, 255);
    SDL_RenderFillRect(renderer, &header);
    int tabX = header.x + 4;
    int tabY = header.y + 4;
    terminal_reset_tab_rects();
    int sessionCount = terminal_session_count();
    int activeIdx = terminal_active_index();
    // Close button (for interactive tabs) placed on the far left
    int closeW = headerH - 8;
    SDL_Rect closeRect = { header.x + 4, tabY, closeW, headerH - 8 };
    terminal_set_close_rect(closeRect);
    SDL_SetRenderDrawColor(renderer, 80, 50, 50, 255);
    SDL_RenderFillRect(renderer, &closeRect);
    SDL_SetRenderDrawColor(renderer, 20, 20, 25, 255);
    SDL_RenderDrawRect(renderer, &closeRect);
    drawText(closeRect.x + closeRect.w / 2 - 4, closeRect.y + (closeRect.h - 14) / 2, "x");

    // Plus button for interactive, next to close
    SDL_Rect plus = { closeRect.x + closeRect.w + 6, tabY, headerH - 8, headerH - 8 };
    terminal_set_plus_rect(plus);
    SDL_SetRenderDrawColor(renderer, 60, 60, 70, 255);
    SDL_RenderFillRect(renderer, &plus);
    SDL_SetRenderDrawColor(renderer, 20, 20, 25, 255);
    SDL_RenderDrawRect(renderer, &plus);
    drawText(plus.x + 6, plus.y + (plus.h - 14) / 2, "+");
    tabX = plus.x + plus.w + 6;

    // Render interactive tabs immediately to the right of plus
    for (int i = 0; i < sessionCount; ++i) {
        const char* name = NULL;
        bool isBuild = false, isRun = false;
        if (!terminal_session_info(i, &name, &isBuild, &isRun)) continue;
        if (isBuild || isRun) continue;
        const char* label = name ? name : "Term";
        char truncated[64];
        const bool isActiveTab = (i == activeIdx);
        const char* drawLabel = label;
        if (!isActiveTab) {
            size_t len = strlen(label);
            if (len > 20) {
                size_t copy = 17;
                if (copy > sizeof(truncated) - 4) copy = sizeof(truncated) - 4;
                memcpy(truncated, label, copy);
                truncated[copy] = '\0';
                strncat(truncated, "...", sizeof(truncated) - copy - 1);
                drawLabel = truncated;
            }
        }
        int textW = getTextWidth(drawLabel);
        int tabW = textW + 16;
        SDL_Rect tabRect = { tabX, tabY, tabW, headerH - 8 };
        terminal_set_tab_rect(i, tabRect);
        if (i == activeIdx) {
            SDL_SetRenderDrawColor(renderer, 70, 90, 140, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 60, 60, 70, 255);
        }
        SDL_RenderFillRect(renderer, &tabRect);
        SDL_SetRenderDrawColor(renderer, 20, 20, 25, 255);
        SDL_RenderDrawRect(renderer, &tabRect);
        drawText(tabRect.x + 8, tabRect.y + (tabRect.h - 14) / 2, drawLabel);
        tabX += tabW + 6;
    }

    int rightStart = header.x + header.w - 4;
    // Render task tabs (Build, Run) on the right side
    for (int i = sessionCount - 1; i >= 0; --i) {
        const char* name = NULL;
        bool isBuild = false, isRun = false;
        if (!terminal_session_info(i, &name, &isBuild, &isRun)) continue;
        if (!isBuild && !isRun) continue;
        const char* label = name ? name : (isBuild ? "Build" : (isRun ? "Run" : "Task"));
        int textW = getTextWidth(label);
        int tabW = textW + 16;
        rightStart -= (tabW + 6);
        SDL_Rect tabRect = { rightStart, tabY, tabW, headerH - 8 };
        terminal_set_tab_rect(i, tabRect);
        if (i == activeIdx) {
            SDL_SetRenderDrawColor(renderer, 90, 110, 160, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 70, 70, 80, 255);
        }
        SDL_RenderFillRect(renderer, &tabRect);
        SDL_SetRenderDrawColor(renderer, 20, 20, 25, 255);
        SDL_RenderDrawRect(renderer, &tabRect);
        drawText(tabRect.x + 8, tabRect.y + (tabRect.h - 14) / 2, label);
    }

    TermGrid* grid = terminal_active_grid();
    if (!grid) return;

    terminal_resize_grid_for_pane(viewport.w, viewport.h);

    PaneScrollState* scroll = terminal_get_scroll_state();
    scroll_state_set_viewport(scroll, (float)viewport.h);
    // Use last used row to size scroll content so we don't pad with empty rows
    int lastRowUsed = terminal_last_used_row();
    int contentRows = (lastRowUsed >= 0) ? (lastRowUsed + 1) : 1;
    int cellH = terminal_cell_height();
    int cellW = terminal_cell_width();
    float contentHeight = (float)cellH * (float)contentRows;
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
    int firstRow = (cellH > 0) ? (int)(offset / (float)cellH) : 0;
    if (firstRow < 0) firstRow = 0;
    if (firstRow > grid->rows) firstRow = grid->rows;
    float intraLineOffset = offset - (float)firstRow * (float)cellH;

    pushClipRect(&viewport);

    int rowsToRender = (viewport.h > 0 && cellH > 0)
        ? ((viewport.h + cellH - 1) / cellH)
        : grid->rows;
    int cols = grid->cols;
    if (!grid->cells || grid->rows <= 0 || grid->cols <= 0) {
        popClipRect();
        return;
    }
    int selStartLine = 0, selStartCol = 0, selEndLine = 0, selEndCol = 0;
    bool hasSelection = terminal_get_selection_bounds(&selStartLine, &selStartCol, &selEndLine, &selEndCol);

    char* lineBuf = (char*)malloc((size_t)cols + 1);
    if (lineBuf) {
        for (int r = 0; r < rowsToRender; ++r) {
            int rowIndex = firstRow + r;
            if (rowIndex >= grid->rows) break;

            float drawYf = (float)viewport.y + (float)r * (float)cellH - intraLineOffset;
            if (drawYf < (float)viewport.y) continue; // avoid drawing rows that would clip above
            if (drawYf >= (float)(viewport.y + viewport.h)) break;

            int drawY = (int)drawYf;
            int lastNonSpace = -1;
            for (int c = 0; c < cols; ++c) {
                TermCell* cell = term_grid_cell(grid, rowIndex, c);
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
            // Selection highlight using grid coords
            if (hasSelection && rowIndex >= selStartLine && rowIndex <= selEndLine) {
                int startCol = (rowIndex == selStartLine) ? selStartCol : 0;
                int endCol = (rowIndex == selEndLine) ? selEndCol : cols;
                if (startCol < 0) startCol = 0;
                if (endCol < startCol) endCol = startCol;
                if (endCol > cols) endCol = cols;
                if (startCol != endCol) {
                    int startX = viewport.x + startCol * cellW;
                    int width = (endCol - startCol) * cellW;
                    SDL_Rect highlight = { startX, drawY, width, cellH };
                    SDL_SetRenderDrawColor(renderer, 80, 120, 200, 100);
                    SDL_RenderFillRect(renderer, &highlight);
                }
            }
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
        int caretRow = grid->cursor_row;
        int caretCol = grid->cursor_col;
        if (caretRow < 0) caretRow = 0;
        if (caretRow >= grid->rows) caretRow = grid->rows - 1;
        if (caretCol < 0) caretCol = 0;
        if (caretCol > cols) caretCol = cols;

        // Build a row slice up to caretCol to measure width with proportional font.
        int caretLen = caretCol;
        if (caretLen > cols) caretLen = cols;
        char* caretBuf = (char*)malloc((size_t)caretLen + 1);
        int caretWidthPx = 0;
        if (caretBuf) {
            for (int c = 0; c < caretLen; ++c) {
                TermCell* cell = term_grid_cell(grid, caretRow, c);
                char ch = cell ? (char)(cell->ch ? cell->ch : ' ') : ' ';
                caretBuf[c] = (ch >= 0x20 && ch < 0x7F) ? ch : ' ';
            }
            caretBuf[caretLen] = '\0';
            caretWidthPx = getTextWidth(caretBuf);
            free(caretBuf);
        }

        int caretX = viewport.x + caretWidthPx;
        int caretY = viewport.y + (caretRow - firstRow) * cellH - (int)intraLineOffset;
        int caretW = (cellW > 0) ? (cellW / 4) : 4;
        int caretH = 3;
        caretY += (cellH > caretH) ? (cellH - caretH) : 0;
        SDL_Rect caret = { caretX, caretY, caretW, caretH };
        SDL_SetRenderDrawColor(renderer, 200, 200, 220, 220);
        SDL_RenderFillRect(renderer, &caret);
    }
}
