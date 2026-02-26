#include "ide/Panes/Terminal/render_terminal.h"
#include "engine/Render/render_helpers.h"
#include "engine/Render/render_text_helpers.h"  // renderUIPane, drawText
#include "engine/Render/render_font.h"
#include "ide/UI/ui_selection_style.h"
#include "app/GlobalInfo/system_control.h"
#include "app/GlobalInfo/core_state.h"

#include "../Terminal/terminal.h"
#include "core/TextSelection/text_selection_manager.h"
#include "ide/Panes/Terminal/terminal_grid.h"
#include "ide/Panes/Terminal/terminal.h" // for globals exported from terminal.c
#include "ide/UI/scroll_manager.h"
#include "ide/Panes/PaneInfo/pane.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int encode_codepoint_utf8(uint32_t cp, char out[4]) {
    if (cp <= 0x7Fu) {
        out[0] = (char)cp;
        return 1;
    }
    if (cp <= 0x7FFu) {
        out[0] = (char)(0xC0u | ((cp >> 6) & 0x1Fu));
        out[1] = (char)(0x80u | (cp & 0x3Fu));
        return 2;
    }
    if (cp <= 0xFFFFu) {
        out[0] = (char)(0xE0u | ((cp >> 12) & 0x0Fu));
        out[1] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
        out[2] = (char)(0x80u | (cp & 0x3Fu));
        return 3;
    }
    if (cp <= 0x10FFFFu) {
        out[0] = (char)(0xF0u | ((cp >> 18) & 0x07u));
        out[1] = (char)(0x80u | ((cp >> 12) & 0x3Fu));
        out[2] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
        out[3] = (char)(0x80u | (cp & 0x3Fu));
        return 4;
    }
    out[0] = '?';
    return 1;
}

void renderTerminalContents(UIPane* pane, bool hovered, struct IDECoreState* core) {

    RenderContext* ctx = getRenderContext();
    if (!ctx || !ctx->renderer) return;
    SDL_Renderer* renderer = ctx->renderer;

    renderUIPane(pane, hovered);

    // Keep terminal visually distinct from generic pane backgrounds.
    SDL_Rect paneBody = {
        .x = pane->x + 1,
        .y = pane->y + 1,
        .w = pane->w - 2,
        .h = pane->h - 2
    };
    if (paneBody.w > 0 && paneBody.h > 0) {
        SDL_SetRenderDrawColor(renderer, 12, 12, 14, 255);
        SDL_RenderFillRect(renderer, &paneBody);
    }

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

    if (viewport.w > 0 && viewport.h > 0) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderFillRect(renderer, &viewport);
    }

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
    drawTextWithTier(closeRect.x + closeRect.w / 2 - 4,
                     closeRect.y + (closeRect.h - 14) / 2,
                     "x",
                     CORE_FONT_TEXT_SIZE_CAPTION);

    // Plus button for interactive, next to close
    SDL_Rect plus = { closeRect.x + closeRect.w + 6, tabY, headerH - 8, headerH - 8 };
    terminal_set_plus_rect(plus);
    SDL_SetRenderDrawColor(renderer, 60, 60, 70, 255);
    SDL_RenderFillRect(renderer, &plus);
    SDL_SetRenderDrawColor(renderer, 20, 20, 25, 255);
    SDL_RenderDrawRect(renderer, &plus);
    drawTextWithTier(plus.x + 6,
                     plus.y + (plus.h - 14) / 2,
                     "+",
                     CORE_FONT_TEXT_SIZE_CAPTION);
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

    TerminalDebugStats stats = {0};
    bool haveStats = terminal_get_debug_stats(&stats);
    bool usingAlternate = haveStats ? stats.using_alternate : false;

    PaneScrollState* scroll = terminal_get_scroll_state();
    scroll_state_set_viewport(scroll, (float)viewport.h);
    int contentRows = terminal_projection_row_count();
    if (usingAlternate) {
        int altRows = grid->viewport_rows;
        if (altRows < 1 || altRows > grid->rows) altRows = grid->rows;
        if (altRows < 1) altRows = 1;
        contentRows = altRows;
    }
    int cellH = terminal_cell_height();
    int cellW = terminal_cell_width();
    float contentHeight = (float)cellH * (float)contentRows;
    scroll_state_set_content_height(scroll, contentHeight);

    if (usingAlternate) {
        scroll->offset_px = 0.0f;
        scroll->target_offset_px = 0.0f;
    } else if (terminal_is_following_output()) {
        float maxOffset = contentHeight - scroll->viewport_height_px;
        if (maxOffset < 0.0f) maxOffset = 0.0f;
        scroll->target_offset_px = maxOffset;
        scroll->offset_px = maxOffset;
    } else {
        scroll_state_clamp(scroll);
    }

    float offset = scroll_state_get_offset(scroll);
    if (usingAlternate) offset = 0.0f;
    int firstRow = (cellH > 0) ? (int)(offset / (float)cellH) : 0;
    if (firstRow < 0) firstRow = 0;
    if (firstRow > contentRows) firstRow = contentRows;
    float intraLineOffset = offset - (float)firstRow * (float)cellH;
    float shortContentPad = 0.0f;
    if (contentHeight < scroll->viewport_height_px) {
        shortContentPad = scroll->viewport_height_px - contentHeight;
    }

    pushClipRect(&viewport);

    int rowsToRender = (viewport.h > 0 && cellH > 0)
        ? ((viewport.h + cellH - 1) / cellH + 1)
        : contentRows;
    int cols = grid->cols;
    if (!grid->cells || grid->rows <= 0 || grid->cols <= 0) {
        popClipRect();
        return;
    }
    int selStartLine = 0, selStartCol = 0, selEndLine = 0, selEndCol = 0;
    bool hasSelection = !usingAlternate &&
                        terminal_get_selection_bounds(&selStartLine, &selStartCol, &selEndLine, &selEndCol);
    TTF_Font* font = getTerminalFont();
    int fontHeight = font ? TTF_FontHeight(font) : cellH;
    if (fontHeight <= 0) fontHeight = cellH;
    int textYOffset = (cellH - fontHeight) / 2;
    if (textYOffset < 0) textYOffset = 0;
    for (int r = 0; r < rowsToRender; ++r) {
        int rowIndex = firstRow + r;
        if (rowIndex >= contentRows) break;

        float drawYf = (float)viewport.y + shortContentPad + (float)r * (float)cellH - intraLineOffset;
        if (drawYf < (float)viewport.y) continue;
        if (drawYf >= (float)(viewport.y + viewport.h)) break;

        int drawY = (int)drawYf;

        SDL_Rect emptyRow = { viewport.x, drawY, cols * cellW, cellH };
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderFillRect(renderer, &emptyRow);

        // Selection highlight using grid coords.
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
                SDL_Color sel = ui_selection_fill_color();
                SDL_SetRenderDrawColor(renderer, sel.r, sel.g, sel.b, sel.a);
                SDL_RenderFillRect(renderer, &highlight);
            }
        }

        if (!font) continue;
        if (usingAlternate) {
            char* runBuf = (char*)malloc((size_t)cols * 4u + 1u);
            if (!runBuf) continue;
            int outLen = 0;
            for (int c = 0; c < cols; ++c) {
                const TermCell* cell = term_grid_cell(grid, rowIndex, c);
                uint32_t cp = cell ? cell->ch : (uint32_t)' ';
                if (cp == 0u) cp = (uint32_t)' ';
                char enc[4];
                int encLen = encode_codepoint_utf8(cp, enc);
                if (outLen + encLen >= cols * 4) break;
                for (int i = 0; i < encLen; ++i) runBuf[outLen++] = enc[i];
            }
            runBuf[outLen] = '\0';
            if (outLen > 0) {
                int textY = drawY + textYOffset;
                SDL_Color fgColor = {220, 220, 226, 255};
                drawTextUTF8WithFontColor(viewport.x, textY, runBuf, font, fgColor, false);
            }
            free(runBuf);
        } else {
            int lineLen = terminal_line_length(rowIndex, false);
            if (lineLen > 0) {
                // `lineLen` is in terminal cells, but UTF-8 output can need up to 4 bytes/cell.
                int cap = cols * 4 + 1;
                if (cap < lineLen + 1) cap = lineLen + 1;
                char* lineBuf = (char*)malloc((size_t)cap);
                if (lineBuf) {
                    terminal_line_to_string(rowIndex, lineBuf, cap, false);
                    SDL_Color fgColor = {220, 220, 226, 255};
                    int textY = drawY + textYOffset;
                    drawTextUTF8WithFontColor(viewport.x, textY, lineBuf, font, fgColor, false);
                    free(lineBuf);
                }
            }
        }
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
        int caretRow = haveStats ? stats.cursor_row : grid->cursor_row;
        int caretCol = haveStats ? stats.cursor_col : grid->cursor_col;
        if (!usingAlternate) {
            int viewportRows = grid->viewport_rows;
            if (viewportRows < 1 || viewportRows > grid->rows) viewportRows = grid->rows;
            int usedRows = grid->used_rows;
            if (usedRows < 1) usedRows = 1;
            if (usedRows > grid->rows) usedRows = grid->rows;
            int viewportStart = usedRows - viewportRows;
            if (viewportStart < 0) viewportStart = 0;
            int localRow = caretRow - viewportStart;
            if (localRow < 0) localRow = 0;
            if (localRow >= viewportRows) localRow = viewportRows - 1;
            caretRow = stats.scrollback_rows + localRow;
        }
        if (caretRow < 0) caretRow = 0;
        if (caretRow >= contentRows) caretRow = contentRows - 1;
        if (caretCol < 0) caretCol = 0;
        if (caretCol > cols) caretCol = cols;

        int caretX = viewport.x + caretCol * cellW;
        int caretY = viewport.y + (int)shortContentPad + (caretRow - firstRow) * cellH - (int)intraLineOffset;
        int caretW = 2;
        if (caretW > cellW) caretW = cellW;
        int caretH = cellH - 2;
        if (caretH < 2) caretH = 2;
        caretY += 1;
        if (caretY >= viewport.y && caretY < viewport.y + viewport.h) {
            SDL_Rect caret = { caretX, caretY, caretW, caretH };
            SDL_SetRenderDrawColor(renderer, 200, 200, 220, 220);
            SDL_RenderFillRect(renderer, &caret);
        }
    }

    if (terminal_debug_overlay_enabled()) {
        TerminalDebugStats stats = {0};
        if (terminal_get_debug_stats(&stats)) {
            char lineA[128];
            char lineB[128];
            snprintf(lineA, sizeof(lineA),
                     "mode:%s cursor:%d,%d viewport:%dx%d",
                     stats.using_alternate ? "alt" : "primary",
                     stats.cursor_row, stats.cursor_col,
                     stats.viewport_rows, stats.viewport_cols);
            snprintf(lineB, sizeof(lineB),
                     "scrollback:%d projected:%d",
                     stats.scrollback_rows, stats.projected_rows);
            SDL_Rect overlay = { viewport.x + 8, viewport.y + 8, 280, 34 };
            SDL_SetRenderDrawColor(renderer, 20, 26, 34, 220);
            SDL_RenderFillRect(renderer, &overlay);
            SDL_SetRenderDrawColor(renderer, 90, 120, 180, 220);
            SDL_RenderDrawRect(renderer, &overlay);
            drawTextWithTier(overlay.x + 6, overlay.y + 3, lineA, CORE_FONT_TEXT_SIZE_CAPTION);
            drawTextWithTier(overlay.x + 6, overlay.y + 17, lineB, CORE_FONT_TEXT_SIZE_CAPTION);
        }
    }
}
