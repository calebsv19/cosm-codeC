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

#define TERMINAL_ATTR_BOLD      (1 << 0)
#define TERMINAL_ATTR_UNDERLINE (1 << 1)
#define TERMINAL_ATTR_WIDE_CONTINUATION (1 << 7)
#define TERMINAL_DEFAULT_BG     0x000000FFu

static SDL_Color term_color_from_rgba(uint32_t packed) {
    SDL_Color color = {
        (Uint8)((packed >> 24) & 0xFFu),
        (Uint8)((packed >> 16) & 0xFFu),
        (Uint8)((packed >> 8) & 0xFFu),
        (Uint8)(packed & 0xFFu)
    };
    return color;
}

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

static bool terminal_cells_same_style(const TermCell* a, const TermCell* b) {
    if (!a || !b) return false;
    return a->fg == b->fg &&
           a->bg == b->bg &&
           a->attrs == b->attrs;
}

static bool terminal_codepoint_is_combining(uint32_t cp) {
    return (cp >= 0x0300u && cp <= 0x036Fu) ||
           (cp >= 0x1AB0u && cp <= 0x1AFFu) ||
           (cp >= 0x1DC0u && cp <= 0x1DFFu) ||
           (cp >= 0x20D0u && cp <= 0x20FFu) ||
           (cp >= 0xFE20u && cp <= 0xFE2Fu);
}

static bool terminal_codepoint_is_box_drawing(uint32_t cp) {
    return cp >= 0x2500u && cp <= 0x257Fu;
}

static bool terminal_cell_is_visible_text(const TermCell* cell) {
    if (!cell) return false;
    uint32_t cp = cell->ch;
    return cp != 0u &&
           cp != (uint32_t)' ' &&
           cp >= 0x20u &&
           cp != 0x7Fu &&
           !terminal_codepoint_is_combining(cp) &&
           (cell->attrs & TERMINAL_ATTR_WIDE_CONTINUATION) == 0;
}

static void terminal_draw_box_drawing_cell(SDL_Renderer* renderer,
                                           int x,
                                           int y,
                                           int cellW,
                                           int cellH,
                                           SDL_Color color,
                                           uint32_t cp) {
    if (!renderer || cellW <= 0 || cellH <= 0) return;

    bool left = false;
    bool right = false;
    bool up = false;
    bool down = false;
    bool thick = false;

    switch (cp) {
        case 0x2500: right = left = true; break;              // ─
        case 0x2501: right = left = true; thick = true; break; // ━
        case 0x2502: up = down = true; break;                 // │
        case 0x2503: up = down = true; thick = true; break;    // ┃
        case 0x250C: right = down = true; break;              // ┌
        case 0x2510: left = down = true; break;               // ┐
        case 0x2514: right = up = true; break;                // └
        case 0x2518: left = up = true; break;                 // ┘
        case 0x251C: right = up = down = true; break;         // ├
        case 0x2524: left = up = down = true; break;          // ┤
        case 0x252C: left = right = down = true; break;       // ┬
        case 0x2534: left = right = up = true; break;         // ┴
        case 0x253C: left = right = up = down = true; break;  // ┼
        case 0x2574: left = true; break;                      // ╴
        case 0x2575: up = true; break;                        // ╵
        case 0x2576: right = true; break;                     // ╶
        case 0x2577: down = true; break;                      // ╷
        case 0x2578: left = true; thick = true; break;
        case 0x2579: up = true; thick = true; break;
        case 0x257A: right = true; thick = true; break;
        case 0x257B: down = true; thick = true; break;
        default:
            if (cp >= 0x2550u && cp <= 0x256Cu) {
                left = right = up = down = true;
            } else {
                left = right = true;
            }
            break;
    }

    int cx = x + cellW / 2;
    int cy = y + cellH / 2;
    int minX = x + 1;
    int maxX = x + cellW - 2;
    int minY = y + 2;
    int maxY = y + cellH - 3;
    if (maxX < minX) maxX = minX;
    if (maxY < minY) maxY = minY;

    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    for (int pass = 0; pass < (thick ? 2 : 1); ++pass) {
        int off = pass;
        if (left) SDL_RenderDrawLine(renderer, minX, cy + off, cx, cy + off);
        if (right) SDL_RenderDrawLine(renderer, cx, cy + off, maxX, cy + off);
        if (up) SDL_RenderDrawLine(renderer, cx + off, minY, cx + off, cy);
        if (down) SDL_RenderDrawLine(renderer, cx + off, cy, cx + off, maxY);
    }
}

static bool terminal_font_has_glyph(TTF_Font* font, uint32_t cp) {
    if (!font) return false;
#if SDL_TTF_VERSION_ATLEAST(2, 0, 18)
    return TTF_GlyphIsProvided32(font, (Uint32)cp) != 0;
#else
    if (cp > 0xFFFFu) return false;
    return TTF_GlyphIsProvided(font, (Uint16)cp) != 0;
#endif
}

static bool terminal_cell_needs_fixed_glyph_draw(TTF_Font* font, const TermCell* cell) {
    if (!terminal_cell_is_visible_text(cell)) return false;
    uint32_t cp = cell->ch;
    if (terminal_codepoint_is_box_drawing(cp)) return true;
    return !terminal_font_has_glyph(font, cp);
}

static void terminal_draw_cell_glyph(SDL_Renderer* renderer,
                                     TTF_Font* font,
                                     int x,
                                     int y,
                                     int textYOffset,
                                     int cellW,
                                     int cellH,
                                     const TermCell* cell) {
    if (!renderer || !font || !cell) return;

    uint32_t cp = cell->ch;
    if (cp == 0u || cp == (uint32_t)' ' || cp < 0x20u || cp == 0x7Fu) return;
    if (terminal_codepoint_is_combining(cp)) return;

    SDL_Color fg = term_color_from_rgba(cell->fg);
    if (terminal_codepoint_is_box_drawing(cp)) {
        terminal_draw_box_drawing_cell(renderer, x, y, cellW, cellH, fg, cp);
        return;
    }

    bool bold = (cell->attrs & TERMINAL_ATTR_BOLD) != 0;
    char enc[4];
    int encLen = 0;
    if (!terminal_font_has_glyph(font, cp)) {
        cp = 0x25A1u; // white square fallback keeps unsupported glyphs cell-aligned.
        if (!terminal_font_has_glyph(font, cp)) cp = (uint32_t)'?';
    }
    encLen = encode_codepoint_utf8(cp, enc);
    char text[5] = {0};
    for (int i = 0; i < encLen; ++i) text[i] = enc[i];
    text[encLen] = '\0';
    drawTextUTF8WithFontColor(x, y + textYOffset, text, font, fg, bold);
}

static uint32_t terminal_run_codepoint(const TermCell* cell) {
    if (!cell) return (uint32_t)' ';
    uint32_t cp = cell->ch;
    if (cp == 0u || cp < 0x20u || cp == 0x7Fu || terminal_codepoint_is_combining(cp)) {
        return (uint32_t)' ';
    }
    if ((cell->attrs & TERMINAL_ATTR_WIDE_CONTINUATION) != 0) {
        return (uint32_t)' ';
    }
    return cp;
}

static void terminal_draw_text_run(SDL_Renderer* renderer,
                                   TTF_Font* font,
                                   const SDL_Rect* viewport,
                                   int rowIndex,
                                   int drawY,
                                   int textYOffset,
                                   int cellW,
                                   int startCol,
                                   int endCol) {
    if (!renderer || !font || !viewport || startCol < 0 || endCol <= startCol) return;

    const TermCell* first = terminal_projection_rowcol_to_cell(rowIndex, startCol);
    if (!first) return;

    size_t cap = (size_t)(endCol - startCol) * 4u + 1u;
    char* text = (char*)malloc(cap);
    if (!text) return;

    size_t outLen = 0;
    for (int c = startCol; c < endCol; ++c) {
        const TermCell* cell = terminal_projection_rowcol_to_cell(rowIndex, c);
        uint32_t cp = terminal_run_codepoint(cell);
        char enc[4];
        int encLen = encode_codepoint_utf8(cp, enc);
        if (outLen + (size_t)encLen >= cap) break;
        for (int i = 0; i < encLen; ++i) {
            text[outLen++] = enc[i];
        }
    }
    text[outLen] = '\0';

    if (outLen > 0) {
        SDL_Color fg = term_color_from_rgba(first->fg);
        bool bold = (first->attrs & TERMINAL_ATTR_BOLD) != 0;
        drawTextUTF8WithFontColor(viewport->x + startCol * cellW,
                                  drawY + textYOffset,
                                  text,
                                  font,
                                  fg,
                                  bold);
    }

    free(text);
}

static void render_terminal_cell_row(SDL_Renderer* renderer,
                                     TTF_Font* font,
                                     const SDL_Rect* viewport,
                                     int rowIndex,
                                     int drawY,
                                     int textYOffset,
                                     int cols,
                                     int cellW,
                                     int cellH) {
    if (!renderer || !font || !viewport || cols <= 0 || cellW <= 0 || cellH <= 0) return;

    int c = 0;
    while (c < cols) {
        const TermCell* first = terminal_projection_rowcol_to_cell(rowIndex, c);
        if (!first) {
            c++;
            continue;
        }

        int runStart = c;
        int runEnd = c + 1;
        while (runEnd < cols) {
            const TermCell* next = terminal_projection_rowcol_to_cell(rowIndex, runEnd);
            if (!terminal_cells_same_style(first, next)) break;
            runEnd++;
        }

        int runCells = runEnd - runStart;
        if (first->bg != TERMINAL_DEFAULT_BG) {
            SDL_Color bg = term_color_from_rgba(first->bg);
            SDL_Rect bgRect = {
                viewport->x + runStart * cellW,
                drawY,
                runCells * cellW,
                cellH
            };
            SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, bg.a);
            SDL_RenderFillRect(renderer, &bgRect);
        }

        if ((first->attrs & TERMINAL_ATTR_UNDERLINE) != 0) {
            SDL_Color fg = term_color_from_rgba(first->fg);
            int underlineY = drawY + cellH - 3;
            if (underlineY < drawY) underlineY = drawY + cellH - 1;
            SDL_SetRenderDrawColor(renderer, fg.r, fg.g, fg.b, fg.a);
            SDL_RenderDrawLine(renderer,
                               viewport->x + runStart * cellW,
                               underlineY,
                               viewport->x + runEnd * cellW - 1,
                               underlineY);
        }

        c = runEnd;
    }

    c = 0;
    while (c < cols) {
        const TermCell* cell = terminal_projection_rowcol_to_cell(rowIndex, c);
        if (!cell) {
            c++;
            continue;
        }
        if (terminal_cell_needs_fixed_glyph_draw(font, cell)) {
            terminal_draw_cell_glyph(renderer,
                                     font,
                                     viewport->x + c * cellW,
                                     drawY,
                                     textYOffset,
                                     cellW,
                                     cellH,
                                     cell);
            c++;
            continue;
        }
        if (!terminal_cell_is_visible_text(cell)) {
            c++;
            continue;
        }

        int runStart = c;
        int runEnd = c + 1;
        while (runEnd < cols) {
            const TermCell* next = terminal_projection_rowcol_to_cell(rowIndex, runEnd);
            if (!terminal_cells_same_style(cell, next)) break;
            if (terminal_cell_needs_fixed_glyph_draw(font, next)) break;
            runEnd++;
        }
        terminal_draw_text_run(renderer,
                               font,
                               viewport,
                               rowIndex,
                               drawY,
                               textYOffset,
                               cellW,
                               runStart,
                               runEnd);
        c = runEnd;
    }
}

static void terminal_apply_scroll_layout(PaneScrollState* scroll,
                                         float viewportHeight,
                                         float contentHeight,
                                         bool usingAlternate,
                                         bool followingOutput) {
    if (!scroll) return;

    float preservedOffset = scroll_state_get_offset(scroll);
    scroll_state_set_viewport(scroll, viewportHeight);
    scroll_state_set_content_height(scroll, contentHeight);

    if (usingAlternate) {
        scroll->offset_px = 0.0f;
        scroll->target_offset_px = 0.0f;
        return;
    }

    float maxOffset = contentHeight - scroll->viewport_height_px;
    if (maxOffset < 0.0f) maxOffset = 0.0f;

    if (followingOutput) {
        scroll->target_offset_px = maxOffset;
        scroll->offset_px = maxOffset;
        return;
    }

    scroll->offset_px = preservedOffset;
    scroll->target_offset_px = preservedOffset;
    scroll_state_clamp(scroll);
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
    terminal_apply_scroll_layout(scroll,
                                 (float)viewport.h,
                                 contentHeight,
                                 usingAlternate,
                                 terminal_is_following_output());

    float offset = scroll_state_get_offset(scroll);
    if (usingAlternate) offset = 0.0f;
    int rowsToFit = (viewport.h > 0 && cellH > 0) ? ((viewport.h + cellH - 1) / cellH) : 1;
    if (rowsToFit < 1) rowsToFit = 1;
    int maxFirstRow = contentRows - rowsToFit;
    if (maxFirstRow < 0) maxFirstRow = 0;
    int firstRow = (cellH > 0) ? (int)(offset / (float)cellH) : 0;
    if (!usingAlternate && scroll && terminal_is_following_output()) {
        firstRow = maxFirstRow;
    }
    if (firstRow < 0) firstRow = 0;
    if (firstRow > maxFirstRow) firstRow = maxFirstRow;
    offset = (float)firstRow * (float)cellH;
    float intraLineOffset = 0.0f;
    float shortContentPad = 0.0f;

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
        render_terminal_cell_row(renderer,
                                 font,
                                 &viewport,
                                 rowIndex,
                                 drawY,
                                 textYOffset,
                                 cols,
                                 cellW,
                                 cellH);
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

    if (paneActive && (!haveStats || stats.cursor_visible)) {
        int caretRow = 0;
        int caretCol = 0;
        bool caretVisible = terminal_cursor_projection_position(&caretRow, &caretCol);
        if (caretRow < firstRow || caretRow >= firstRow + rowsToRender) caretVisible = false;
        if (caretCol < 0) caretCol = 0;
        if (caretCol > cols) caretCol = cols;

        if (caretVisible) {
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
    }

    if (terminal_debug_overlay_enabled()) {
        TerminalDebugStats stats = {0};
        if (terminal_get_debug_stats(&stats)) {
            char lineA[128];
            char lineB[128];
            char lineC[160];
            snprintf(lineA, sizeof(lineA),
                     "mode:%s cursor:%d,%d viewport:%dx%d",
                     stats.using_alternate ? "alt" : "primary",
                     stats.cursor_row, stats.cursor_col,
                     stats.viewport_rows, stats.viewport_cols);
            snprintf(lineB, sizeof(lineB),
                     "journal:%d scrollback:%d projected:%d",
                     stats.journal_rows, stats.scrollback_rows, stats.projected_rows);
            snprintf(lineC, sizeof(lineC),
                     "first:%d offset:%.1f follow:%d commits:%llu journal:%llu drops:%llu",
                     firstRow,
                     offset,
                     stats.follow_output ? 1 : 0,
                     stats.scrollback_commits,
                     stats.journal_inserts,
                     stats.journal_drops);
            SDL_Rect overlay = { viewport.x + 8, viewport.y + 8, 520, 50 };
            SDL_SetRenderDrawColor(renderer, 20, 26, 34, 220);
            SDL_RenderFillRect(renderer, &overlay);
            SDL_SetRenderDrawColor(renderer, 90, 120, 180, 220);
            SDL_RenderDrawRect(renderer, &overlay);
            drawTextWithTier(overlay.x + 6, overlay.y + 3, lineA, CORE_FONT_TEXT_SIZE_CAPTION);
            drawTextWithTier(overlay.x + 6, overlay.y + 17, lineB, CORE_FONT_TEXT_SIZE_CAPTION);
            drawTextWithTier(overlay.x + 6, overlay.y + 31, lineC, CORE_FONT_TEXT_SIZE_CAPTION);
        }
    }
}
