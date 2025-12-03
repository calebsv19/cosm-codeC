#include "ide/Panes/Terminal/render_terminal.h"
#include "engine/Render/render_helpers.h"
#include "engine/Render/render_text_helpers.h"  // renderUIPane, drawText
#include "app/GlobalInfo/system_control.h"
#include "app/GlobalInfo/core_state.h"

#include "../Terminal/terminal.h"
#include "core/TextSelection/text_selection_manager.h"
#include "ide/UI/scroll_manager.h"
#include "ide/Panes/PaneInfo/pane.h"

#include <SDL2/SDL.h>
#include <string.h>

void renderTerminalContents(UIPane* pane, bool hovered, struct IDECoreState* core) {

    RenderContext* ctx = getRenderContext();
    if (!ctx || !ctx->renderer) return;
    SDL_Renderer* renderer = ctx->renderer;

    renderUIPane(pane, hovered);

    const int lineHeight = TERMINAL_LINE_HEIGHT;
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
    const char** lines = getTerminalBuffer();

    PaneScrollState* scroll = terminal_get_scroll_state();
    scroll_state_set_viewport(scroll, (float)viewport.h);
    float contentHeight = (float)totalLines * (float)lineHeight;
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
    int firstLine = (lineHeight > 0) ? (int)(offset / (float)lineHeight) : 0;
    if (firstLine < 0) firstLine = 0;
    if (firstLine > totalLines) firstLine = totalLines;
    float intraLineOffset = offset - (float)firstLine * (float)lineHeight;

    pushClipRect(&viewport);

    int selStartLine = 0, selStartCol = 0, selEndLine = 0, selEndCol = 0;
    bool hasSelection = terminal_get_selection_bounds(&selStartLine, &selStartCol, &selEndLine, &selEndCol);

    int linesToRender = (viewport.h > 0) ? (viewport.h / lineHeight) + 2 : totalLines;
    for (int i = 0; i < linesToRender; ++i) {
        int lineIndex = firstLine + i;
        if (lineIndex >= totalLines) break;

        float drawYf = (float)viewport.y + (float)i * (float)lineHeight - intraLineOffset;
        if (drawYf >= (float)(viewport.y + viewport.h)) break;
        if (drawYf + lineHeight <= viewport.y) continue;

        int drawY = (int)drawYf;
        int drawX = viewport.x;
        int maxWidth = viewport.w;

        const char* text = lines[lineIndex];
        if (!text) text = "";

        if (hasSelection && lineIndex >= selStartLine && lineIndex <= selEndLine) {
            int lineLen = (int)strlen(text);
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
        int lineLen = (int)strlen(text);
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
}
