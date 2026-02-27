#include "ide/Panes/ToolPanels/tool_panel_chrome.h"

#include "ide/UI/shared_theme_font_adapter.h"

static Uint8 clamp_u8(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (Uint8)v;
}

static SDL_Color darken_color(SDL_Color c, int amount) {
    return (SDL_Color){
        clamp_u8((int)c.r - amount),
        clamp_u8((int)c.g - amount),
        clamp_u8((int)c.b - amount),
        c.a
    };
}

static bool same_rgb(SDL_Color a, SDL_Color b) {
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

int tool_panel_compute_content_top(const ToolPanelHeaderMetrics* metrics) {
    if (!metrics) return 0;

    int last_info_y = metrics->info_start_y;
    if (metrics->info_line_count > 1) {
        last_info_y += (metrics->info_line_count - 1) * metrics->info_line_gap;
    }
    int computed = last_info_y + metrics->bottom_padding;
    if (computed < metrics->controls_y + metrics->controls_h + metrics->bottom_padding) {
        computed = metrics->controls_y + metrics->controls_h + metrics->bottom_padding;
    }
    if (computed < metrics->min_content_top) {
        computed = metrics->min_content_top;
    }
    return computed;
}

void tool_panel_compute_split_layout(const UIPane* pane, int content_top, ToolPanelSplitLayout* out_layout) {
    if (!pane || !out_layout) return;

    if (content_top < pane->y) content_top = pane->y;
    if (content_top > pane->y + pane->h) content_top = pane->y + pane->h;

    out_layout->header_rect = (SDL_Rect){
        pane->x + 1,
        pane->y,
        pane->w - 2,
        content_top - pane->y
    };
    out_layout->body_rect = (SDL_Rect){
        pane->x + 1,
        content_top,
        pane->w - 2,
        (pane->y + pane->h) - content_top
    };

    if (out_layout->header_rect.w < 0) out_layout->header_rect.w = 0;
    if (out_layout->header_rect.h < 0) out_layout->header_rect.h = 0;
    if (out_layout->body_rect.w < 0) out_layout->body_rect.w = 0;
    if (out_layout->body_rect.h < 0) out_layout->body_rect.h = 0;
}

SDL_Color tool_panel_body_color(const UIPane* pane, int darken_amount) {
    IDEThemePalette palette = {0};
    if (!pane) return (SDL_Color){24, 24, 24, 255};

    if (ide_shared_theme_resolve_palette(&palette)) {
        SDL_Color listBg = palette.pane_body_fill;
        if (same_rgb(listBg, pane->bgColor)) {
            listBg = darken_color(pane->bgColor, darken_amount);
        }
        return listBg;
    }

    {
        SDL_Color editorBg = ide_shared_theme_background_color();
        SDL_Color listBg = editorBg;
        if (same_rgb(listBg, pane->bgColor)) {
            listBg = darken_color(pane->bgColor, darken_amount);
        }
        return listBg;
    }
}

void tool_panel_render_split_background(SDL_Renderer* renderer,
                                        const UIPane* pane,
                                        int content_top,
                                        int darken_amount) {
    if (!renderer || !pane) return;

    ToolPanelSplitLayout layout = {0};
    tool_panel_compute_split_layout(pane, content_top, &layout);
    SDL_Color bodyBg = tool_panel_body_color(pane, darken_amount);

    if (layout.body_rect.w > 0 && layout.body_rect.h > 0) {
        SDL_SetRenderDrawColor(renderer, bodyBg.r, bodyBg.g, bodyBg.b, 255);
        SDL_RenderFillRect(renderer, &layout.body_rect);
    }
}
