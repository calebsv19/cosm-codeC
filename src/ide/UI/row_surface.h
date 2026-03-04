#ifndef IDE_UI_ROW_SURFACE_H
#define IDE_UI_ROW_SURFACE_H

#include "ide/UI/ui_selection_style.h"

#include <SDL2/SDL.h>
#include <stdbool.h>

typedef struct UIRowSurfaceLayout {
    SDL_Rect bounds;
} UIRowSurfaceLayout;

typedef struct UIRowSurfaceRenderSpec {
    bool draw_selection_fill;
    bool draw_selection_outline;
    bool draw_hover_outline;
    bool use_primary_fill;
    SDL_Color primary_fill;
    bool use_secondary_fill;
    SDL_Color secondary_fill;
    bool use_primary_outline;
    SDL_Color primary_outline;
    bool use_secondary_outline;
    SDL_Color secondary_outline;
} UIRowSurfaceRenderSpec;

static inline UIRowSurfaceLayout ui_row_surface_layout_from_rect(SDL_Rect rect) {
    UIRowSurfaceLayout layout = {0};
    layout.bounds = rect;
    return layout;
}

static inline UIRowSurfaceLayout ui_row_surface_layout_from_content(int content_x,
                                                                    int content_y,
                                                                    int content_width,
                                                                    int line_height,
                                                                    int pad_x,
                                                                    int pad_y) {
    UIRowSurfaceLayout layout = {0};
    if (content_width < 0) content_width = 0;
    if (line_height < 0) line_height = 0;
    if (pad_x < 0) pad_x = 0;
    if (pad_y < 0) pad_y = 0;

    layout.bounds.x = content_x - pad_x;
    layout.bounds.y = content_y - pad_y;
    layout.bounds.w = content_width + (pad_x * 2);
    layout.bounds.h = line_height + (pad_y * 2);
    return layout;
}

static inline bool ui_row_surface_contains(const UIRowSurfaceLayout* layout, int x, int y) {
    if (!layout) return false;
    const SDL_Rect* r = &layout->bounds;
    return x >= r->x && x < (r->x + r->w) && y >= r->y && y < (r->y + r->h);
}

static inline bool ui_row_surface_clip(const UIRowSurfaceLayout* layout,
                                       const SDL_Rect* clip_rect,
                                       UIRowSurfaceLayout* out_layout) {
    if (!layout || !clip_rect || !out_layout) return false;
    SDL_Rect clipped = {0};
    if (!SDL_IntersectRect(&layout->bounds, clip_rect, &clipped)) return false;
    out_layout->bounds = clipped;
    return true;
}

static inline void ui_row_surface_draw_selection_fill(SDL_Renderer* renderer,
                                                      const UIRowSurfaceLayout* layout) {
    if (!renderer || !layout) return;
    SDL_Color fill = ui_selection_fill_color();
    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
    SDL_RenderFillRect(renderer, &layout->bounds);
}

static inline void ui_row_surface_draw_selection_outline(SDL_Renderer* renderer,
                                                         const UIRowSurfaceLayout* layout) {
    if (!renderer || !layout) return;
    SDL_Color outline = ui_selection_outline_color();
    SDL_SetRenderDrawColor(renderer, outline.r, outline.g, outline.b, outline.a);
    SDL_RenderDrawRect(renderer, &layout->bounds);
}

static inline void ui_row_surface_draw_hover_outline(SDL_Renderer* renderer,
                                                     const UIRowSurfaceLayout* layout) {
    if (!renderer || !layout) return;
    SDL_SetRenderDrawColor(renderer, 120, 132, 150, 120);
    SDL_RenderDrawRect(renderer, &layout->bounds);
}

static inline void ui_row_surface_draw_selection(SDL_Renderer* renderer,
                                                 const UIRowSurfaceLayout* layout) {
    if (!renderer || !layout) return;
    ui_row_surface_draw_selection_fill(renderer, layout);
    ui_row_surface_draw_selection_outline(renderer, layout);
}

static inline void ui_row_surface_draw_fill_color(SDL_Renderer* renderer,
                                                  const UIRowSurfaceLayout* layout,
                                                  SDL_Color color) {
    if (!renderer || !layout) return;
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(renderer, &layout->bounds);
}

static inline void ui_row_surface_draw_outline_color(SDL_Renderer* renderer,
                                                     const UIRowSurfaceLayout* layout,
                                                     SDL_Color color) {
    if (!renderer || !layout) return;
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderDrawRect(renderer, &layout->bounds);
}

static inline void ui_row_surface_render(SDL_Renderer* renderer,
                                         const UIRowSurfaceLayout* layout,
                                         const UIRowSurfaceRenderSpec* spec) {
    if (!renderer || !layout || !spec) return;

    if (spec->draw_selection_fill) {
        ui_row_surface_draw_selection_fill(renderer, layout);
    }
    if (spec->use_primary_fill) {
        ui_row_surface_draw_fill_color(renderer, layout, spec->primary_fill);
    }
    if (spec->use_secondary_fill) {
        ui_row_surface_draw_fill_color(renderer, layout, spec->secondary_fill);
    }
    if (spec->draw_selection_outline) {
        ui_row_surface_draw_selection_outline(renderer, layout);
    }
    if (spec->use_primary_outline) {
        ui_row_surface_draw_outline_color(renderer, layout, spec->primary_outline);
    }
    if (spec->use_secondary_outline) {
        ui_row_surface_draw_outline_color(renderer, layout, spec->secondary_outline);
    }
    if (spec->draw_hover_outline) {
        ui_row_surface_draw_hover_outline(renderer, layout);
    }
}

#endif
