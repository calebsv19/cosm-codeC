#ifndef IDE_UI_PANEL_CONTROL_WIDGETS_H
#define IDE_UI_PANEL_CONTROL_WIDGETS_H

#include "engine/Render/render_helpers.h"
#include "engine/Render/render_font.h"
#include "engine/Render/render_text_helpers.h"
#include "ide/UI/panel_metrics.h"
#include "ide/UI/shared_theme_font_adapter.h"

#include <SDL2/SDL.h>
#include <stdbool.h>
#include <string.h>

typedef struct UIPanelCompactButtonSpec {
    SDL_Rect rect;
    const char* label;
    bool active;
    bool outlined;
    bool use_custom_fill;
    SDL_Color custom_fill;
    bool use_custom_outline;
    SDL_Color custom_outline;
    CoreFontTextSizeTier tier;
} UIPanelCompactButtonSpec;

typedef struct UIPanelTextFieldButtonStripLayout {
    SDL_Rect text_field_rect;
    SDL_Rect aux_button_rect;
    SDL_Rect trailing_button_rect;
} UIPanelTextFieldButtonStripLayout;

typedef struct UIPanelThreeSegmentStripLayout {
    SDL_Rect leading_rect;
    SDL_Rect middle_rect;
    SDL_Rect trailing_rect;
} UIPanelThreeSegmentStripLayout;

typedef struct UIPanelVerticalButtonStackLayout {
    SDL_Rect button_rects[8];
    int count;
} UIPanelVerticalButtonStackLayout;

typedef struct UIPanelTaggedRect {
    int tag;
    SDL_Rect rect;
} UIPanelTaggedRect;

typedef struct UIPanelTaggedRectList {
    UIPanelTaggedRect* items;
    int count;
    int capacity;
} UIPanelTaggedRectList;

typedef struct UIPanelCompactButtonRowItem {
    int tag;
    const char* label;
    bool active;
    bool outlined;
} UIPanelCompactButtonRowItem;

static inline bool ui_panel_rect_contains(const SDL_Rect* rect, int x, int y) {
    if (!rect || rect->w <= 0 || rect->h <= 0) return false;
    return x >= rect->x &&
           x <= (rect->x + rect->w) &&
           y >= rect->y &&
           y <= (rect->y + rect->h);
}

static inline UIPanelTextFieldButtonStripLayout ui_panel_text_field_button_strip_layout(int x,
                                                                                        int y,
                                                                                        int total_w,
                                                                                        int button_w_a,
                                                                                        int button_w_b,
                                                                                        int gap,
                                                                                        int field_h) {
    UIPanelTextFieldButtonStripLayout layout = {0};
    if (gap < 0) gap = 0;
    if (field_h < 1) field_h = 1;
    if (button_w_a < 0) button_w_a = 0;
    if (button_w_b < 0) button_w_b = 0;

    int field_w = total_w - button_w_a - button_w_b - (2 * gap);
    if (field_w < 0) field_w = 0;

    layout.text_field_rect = (SDL_Rect){ x, y, field_w, field_h };
    layout.aux_button_rect = (SDL_Rect){ x + field_w + gap, y, button_w_a, field_h };
    layout.trailing_button_rect = (SDL_Rect){
        layout.aux_button_rect.x + button_w_a + gap,
        y,
        button_w_b,
        field_h
    };
    return layout;
}

static inline UIPanelThreeSegmentStripLayout ui_panel_three_segment_strip_layout(int x,
                                                                                 int y,
                                                                                 int total_w,
                                                                                 int leading_w,
                                                                                 int trailing_w,
                                                                                 int gap,
                                                                                 int row_h) {
    UIPanelThreeSegmentStripLayout layout = {0};
    if (gap < 0) gap = 0;
    if (row_h < 1) row_h = 1;
    if (leading_w < 0) leading_w = 0;
    if (trailing_w < 0) trailing_w = 0;

    int middle_w = total_w - leading_w - trailing_w - (2 * gap);
    if (middle_w < 0) middle_w = 0;

    layout.leading_rect = (SDL_Rect){ x, y, leading_w, row_h };
    layout.middle_rect = (SDL_Rect){ x + leading_w + gap, y, middle_w, row_h };
    layout.trailing_rect = (SDL_Rect){
        layout.middle_rect.x + middle_w + gap,
        y,
        trailing_w,
        row_h
    };
    return layout;
}

static inline UIPanelVerticalButtonStackLayout ui_panel_vertical_button_stack_layout(int x,
                                                                                     int y,
                                                                                     int button_w,
                                                                                     int button_h,
                                                                                     int gap_y,
                                                                                     int count) {
    UIPanelVerticalButtonStackLayout layout = {0};
    if (button_w < 1) button_w = 1;
    if (button_h < 1) button_h = 1;
    if (gap_y < 0) gap_y = 0;
    if (count < 0) count = 0;
    if (count > (int)(sizeof(layout.button_rects) / sizeof(layout.button_rects[0]))) {
        count = (int)(sizeof(layout.button_rects) / sizeof(layout.button_rects[0]));
    }

    layout.count = count;
    for (int i = 0; i < count; ++i) {
        layout.button_rects[i] = (SDL_Rect){ x, y + (i * (button_h + gap_y)), button_w, button_h };
    }
    return layout;
}

static inline void ui_panel_tagged_rect_list_reset(UIPanelTaggedRectList* list) {
    if (!list) return;
    list->count = 0;
}

static inline bool ui_panel_tagged_rect_list_add(UIPanelTaggedRectList* list, int tag, SDL_Rect rect) {
    if (!list || !list->items || list->capacity <= 0 || list->count >= list->capacity) return false;
    list->items[list->count].tag = tag;
    list->items[list->count].rect = rect;
    list->count++;
    return true;
}

static inline int ui_panel_tagged_rect_list_hit_test(const UIPanelTaggedRectList* list, int x, int y) {
    if (!list || !list->items) return 0;
    for (int i = 0; i < list->count; ++i) {
        if (ui_panel_rect_contains(&list->items[i].rect, x, y)) {
            return list->items[i].tag;
        }
    }
    return 0;
}

static inline void ui_panel_compact_button_render(SDL_Renderer* renderer,
                                                  const UIPanelCompactButtonSpec* spec) {
    if (!renderer || !spec) return;

    IDEThemePalette palette = {0};
    SDL_Color fill = {80, 80, 80, 255};
    SDL_Color fillActive = {120, 120, 120, 255};
    SDL_Color border = {180, 180, 180, 255};
    SDL_Color text = {230, 230, 230, 255};
    ide_shared_theme_resolve_palette(&palette);
    ide_shared_theme_button_colors(&fill, &fillActive, &border, &text);

    SDL_Color bg = spec->use_custom_fill ? spec->custom_fill : (spec->active ? fillActive : fill);
    SDL_Color outline = spec->use_custom_outline ? spec->custom_outline : border;

    SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, bg.a);
    SDL_RenderFillRect(renderer, &spec->rect);
    SDL_SetRenderDrawColor(renderer, outline.r, outline.g, outline.b, 255);
    SDL_RenderDrawRect(renderer, &spec->rect);

    if (spec->outlined) {
        SDL_Rect outer = {
            spec->rect.x - 1,
            spec->rect.y - 1,
            spec->rect.w + 2,
            spec->rect.h + 2
        };
        SDL_SetRenderDrawColor(renderer,
                               palette.accent_warning.r,
                               palette.accent_warning.g,
                               palette.accent_warning.b,
                               palette.accent_warning.a);
        SDL_RenderDrawRect(renderer, &outer);
    }

    const char* src = spec->label ? spec->label : "";
    int textMaxW = spec->rect.w - 8;
    size_t keepLen = getTextClampedLength(src, textMaxW);
    char labelBuf[64];
    if (keepLen >= sizeof(labelBuf)) keepLen = sizeof(labelBuf) - 1;
    memcpy(labelBuf, src, keepLen);
    labelBuf[keepLen] = '\0';

    TTF_Font* tierFont = getUIFontByTier(spec->tier);
    if (!tierFont) {
        tierFont = getActiveFont();
    }
    int textW = getTextWidthWithFont(labelBuf, tierFont);
    int textH = tierFont ? TTF_FontHeight(tierFont) : 12;
    if (textH < 1) textH = 12;

    int tx = spec->rect.x + (spec->rect.w - textW) / 2;
    int ty = spec->rect.y + (spec->rect.h - textH) / 2;
    if (ty < spec->rect.y) {
        ty = spec->rect.y;
    }
    drawTextUTF8WithFontColor(tx,
                              ty,
                              labelBuf,
                              tierFont,
                              palette.text_primary,
                              false);
}

static inline void ui_panel_compact_button_row_render(SDL_Renderer* renderer,
                                                      int x,
                                                      int y,
                                                      int button_w,
                                                      int button_h,
                                                      int gap_x,
                                                      const UIPanelCompactButtonRowItem* items,
                                                      int item_count,
                                                      CoreFontTextSizeTier tier,
                                                      UIPanelTaggedRectList* out_hits) {
    if (!renderer || !items || item_count <= 0) return;
    if (button_w < 1) button_w = 1;
    if (button_h < 1) button_h = 1;
    if (gap_x < 0) gap_x = 0;

    int cx = x;
    for (int i = 0; i < item_count; ++i) {
        SDL_Rect rect = { cx, y, button_w, button_h };
        ui_panel_compact_button_render(renderer,
                                       &(UIPanelCompactButtonSpec){
                                           .rect = rect,
                                           .label = items[i].label,
                                           .active = items[i].active,
                                           .outlined = items[i].outlined,
                                           .use_custom_fill = false,
                                           .use_custom_outline = false,
                                           .tier = tier
                                       });
        if (out_hits && items[i].tag != 0) {
            (void)ui_panel_tagged_rect_list_add(out_hits, items[i].tag, rect);
        }
        cx += button_w + gap_x;
    }
}

static inline int ui_panel_collapsible_header_render(SDL_Renderer* renderer,
                                                     int x,
                                                     int y,
                                                     const char* label,
                                                     bool collapsed,
                                                     CoreFontTextSizeTier tier,
                                                     SDL_Rect* out_hit_rect) {
    (void)renderer;
    char title[128];
    snprintf(title, sizeof(title), "%s %s", collapsed ? "[+]" : "[-]", label ? label : "");
    drawTextWithTier(x, y, title, tier);
    TTF_Font* tierFont = getUIFontByTier(tier);
    if (!tierFont) {
        tierFont = getActiveFont();
    }
    int titleW = getTextWidthWithFont(title, tierFont);
    int titleH = tierFont ? TTF_FontHeight(tierFont) : IDE_UI_DENSE_HEADER_HEIGHT;
    if (titleH < IDE_UI_DENSE_HEADER_HEIGHT) {
        titleH = IDE_UI_DENSE_HEADER_HEIGHT;
    }
    if (out_hit_rect) {
        *out_hit_rect = (SDL_Rect){ x - 2, y - 1, titleW + 8, titleH + 2 };
    }
    return titleW;
}

#endif
