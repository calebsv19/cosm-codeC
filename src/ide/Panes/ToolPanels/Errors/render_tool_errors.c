#include "ide/Panes/ToolPanels/Errors/render_tool_errors.h"
#include "engine/Render/render_helpers.h"
#include "engine/Render/render_font.h"

#include "ide/Panes/ToolPanels/Errors/tool_errors.h"
#include "core/Diagnostics/diagnostics_engine.h"
#include "core/Analysis/analysis_store.h"
#include "engine/Render/render_pipeline.h"
#include "ide/UI/scroll_manager.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>

// Forward from tool_errors.c
bool is_error_selected(int idx);
static SDL_Rect gErrorScrollTrack = {0};
static SDL_Rect gErrorScrollThumb = {0};
static TTF_Font* gErrorSmallFont = NULL;

static TTF_Font* get_error_font(void) {
    if (gErrorSmallFont) return gErrorSmallFont;
    gErrorSmallFont = TTF_OpenFont("include/fonts/Montserrat/Montserrat-Regular.ttf", 12);
    if (!gErrorSmallFont) {
        fprintf(stderr, "Failed to load error panel font: %s\n", TTF_GetError());
    }
    return gErrorSmallFont;
}

void renderErrorsPanel(UIPane* pane) {
    static bool scrollInit = false;
    if (!scrollInit) {
        scroll_state_init(errors_get_scroll_state(), NULL);
        scrollInit = true;
    }

    TTF_Font* font = get_error_font();
    int paddingX = 12;
    int paddingY = 24;
    int lineHeight = font ? TTF_FontHeight(font) : 16;
    int headerHeight = lineHeight;
    int diagHeight = lineHeight * 2;

    int x = pane->x + paddingX;
    int contentTop = pane->y + paddingY;
    int viewportH = pane->h - (contentTop - pane->y);
    if (viewportH < 0) viewportH = 0;
    PaneScrollState* scroll = errors_get_scroll_state();
    scroll_state_set_viewport(scroll, (float)viewportH);

    size_t files = analysis_store_file_count();
    if (files == 0) {
        drawTextWithFont(x, contentTop, "(No errors or warnings)", font ? font : getActiveFont());
        return;
    }

    FlatDiagRef refs[512];
    int flatCount = flatten_diagnostics(refs, 512);

    float contentHeight = 0.0f;
    for (int i = 0; i < flatCount; ++i) {
        contentHeight += refs[i].isHeader ? (float)headerHeight : (float)diagHeight;
    }
    scroll_state_set_content_height(scroll, contentHeight);
    float offset = scroll_state_get_offset(scroll);

    SDL_Rect clip = { pane->x, contentTop, pane->w - 8, viewportH };
    pushClipRect(&clip);

    int y = contentTop - (int)offset;
    int maxY = contentTop + viewportH;

    for (int i = 0; i < flatCount; ++i) {
        int entryHeight = refs[i].isHeader ? headerHeight : diagHeight;
        if (y + entryHeight < contentTop) { y += entryHeight; continue; }
        if (y > maxY) break;

        bool sel = is_error_selected(i);
        if (sel) {
            SDL_Rect highlight = { x - 8, y - 2, clip.w - paddingX + 8, entryHeight };
            SDL_SetRenderDrawColor(getRenderContext()->renderer, 60, 80, 120, 120);
            SDL_RenderFillRect(getRenderContext()->renderer, &highlight);
        }

        if (refs[i].isHeader) {
            drawTextWithFont(x, y, refs[i].path ? refs[i].path : "(unknown file)", font ? font : getActiveFont());
            y += entryHeight;
        } else {
            const Diagnostic* diag = refs[i].diag;
            const char* sev = (diag->severity == DIAG_SEVERITY_ERROR)
                ? "[E]" : (diag->severity == DIAG_SEVERITY_WARNING) ? "[W]" : "[I]";
            char line[1024];
            int labelX = x + 12;
            int msgX   = x + 28;

            snprintf(line, sizeof(line), "%s %d:%d", sev, diag->line, diag->column);
            drawTextWithFont(labelX, y, line, font ? font : getActiveFont());
            y += lineHeight;

            snprintf(line, sizeof(line), "%s", diag->message ? diag->message : "(no message)");
            drawTextWithFont(msgX, y, line, font ? font : getActiveFont());
            y += lineHeight;
        }
    }

    popClipRect();

    bool showScrollbar = scroll_state_can_scroll(scroll) && viewportH > 0;
    if (showScrollbar) {
        gErrorScrollTrack = (SDL_Rect){
            pane->x + pane->w - 8,
            contentTop,
            4,
            viewportH
        };
        gErrorScrollThumb = scroll_state_thumb_rect(scroll,
                                                   gErrorScrollTrack.x,
                                                   gErrorScrollTrack.y,
                                                   gErrorScrollTrack.w,
                                                   gErrorScrollTrack.h);
        SDL_Color trackColor = scroll->track_color;
        SDL_Color thumbColor = scroll->thumb_color;
        SDL_SetRenderDrawColor(getRenderContext()->renderer, trackColor.r, trackColor.g, trackColor.b, trackColor.a);
        SDL_RenderFillRect(getRenderContext()->renderer, &gErrorScrollTrack);
        SDL_SetRenderDrawColor(getRenderContext()->renderer, thumbColor.r, thumbColor.g, thumbColor.b, thumbColor.a);
        SDL_RenderFillRect(getRenderContext()->renderer, &gErrorScrollThumb);
    } else {
        gErrorScrollTrack = (SDL_Rect){0};
        gErrorScrollThumb = (SDL_Rect){0};
    }

    errors_set_scroll_rects(gErrorScrollTrack, gErrorScrollThumb);
}
