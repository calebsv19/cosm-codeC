#include "ide/Panes/ToolPanels/Assets/render_tool_assets.h"
#include "engine/Render/render_helpers.h"
#include "engine/Render/render_pipeline.h"
#include "ide/Panes/ToolPanels/Assets/tool_assets.h"
#include "ide/UI/scroll_manager.h"
#include "ide/UI/shared_theme_font_adapter.h"

#include <SDL2/SDL.h>
#include <stdio.h>

static PaneScrollState gScrollState;
static bool gScrollInit = false;
static SDL_Rect gScrollTrack = {0};
static SDL_Rect gScrollThumb = {0};
static const PaneScrollConfig gScrollCfg = { .line_height_px = 18.0f, .deceleration_px = 0.0f, .allow_negative = false };

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

void renderAssetManagerPanel(UIPane* pane) {
    if (!gScrollInit) {
        scroll_state_init(&gScrollState, &gScrollCfg);
        gScrollInit = true;
    }

    const int headerHeight = 18;
    const int lineHeight = 16;
    const int paddingX = 8;
    const int paddingY = 28;
    const int trackWidth = 6;
    const int trackPadding = 4;

    int contentTop = pane->y + paddingY;
    int viewportH = pane->h - (contentTop - pane->y);
    if (viewportH < 0) viewportH = 0;
    scroll_state_set_viewport(&gScrollState, (float)viewportH);

    SDL_Color editorBg = ide_shared_theme_background_color();
    SDL_Color listBg = editorBg;
    if (same_rgb(listBg, pane->bgColor)) {
        listBg = darken_color(pane->bgColor, 14);
    }
    SDL_Rect bodyBg = {
        pane->x + 1,
        contentTop,
        pane->w - 2,
        viewportH
    };
    SDL_SetRenderDrawColor(getRenderContext()->renderer, listBg.r, listBg.g, listBg.b, 255);
    SDL_RenderFillRect(getRenderContext()->renderer, &bodyBg);

    AssetFlatRef refs[1024];
    int count = assets_flatten(refs, 1024);

    float contentHeight = 0.0f;
    for (int i = 0; i < count; ++i) {
        contentHeight += (refs[i].isHeader ? (float)headerHeight : (float)lineHeight);
    }
    scroll_state_set_content_height(&gScrollState, contentHeight);

    float offset = scroll_state_get_offset(&gScrollState);

    SDL_Rect clipRect = {
        pane->x,
        contentTop,
        pane->w - (trackWidth + trackPadding + 2),
        viewportH
    };
    if (clipRect.w < 0) clipRect.w = 0;
    pushClipRect(&clipRect);

    int x = pane->x + paddingX;
    int y = contentTop - (int)offset;
    int maxY = contentTop + viewportH;

    if (count == 0) {
        drawText(x, y, "(No assets found)");
    } else {
        for (int i = 0; i < count; ++i) {
            int h = refs[i].isHeader ? headerHeight : lineHeight;
            if (y + h > contentTop && y < maxY) {
                if (assets_is_selected(i)) {
                    SDL_Rect rect = { x - 6, y - 2, clipRect.w - paddingX + 4, h };
                    SDL_SetRenderDrawColor(getRenderContext()->renderer, 60, 80, 120, 120);
                    SDL_RenderFillRect(getRenderContext()->renderer, &rect);
                }

                if (refs[i].isHeader) {
                    const AssetCategoryList* list = &assets_get_catalog()->categories[refs[i].category];
                    char buf[128];
                    snprintf(buf, sizeof(buf), "%s (%d)%s",
                             (const char*[]){"Images","Audio","Data","Other"}[refs[i].category],
                             list->count,
                             list->collapsed ? " [collapsed]" : "");
                    drawText(x, y, buf);
                } else if (refs[i].isMoreLine) {
                    const AssetCategoryList* list = &assets_get_catalog()->categories[refs[i].category];
                    int remaining = list->count - ASSET_RENDER_LIMIT_PER_BUCKET;
                    char moreBuf[64];
                    snprintf(moreBuf, sizeof(moreBuf), "... and %d more", remaining > 0 ? remaining : 0);
                    drawText(x + 12, y, moreBuf);
                } else if (refs[i].entry) {
                    const char* label = refs[i].entry->relPath ? refs[i].entry->relPath : refs[i].entry->name;
                    drawText(x + 12, y, label);
                }
            }
            y += h;
        }
    }

    popClipRect();

    bool showScrollbar = scroll_state_can_scroll(&gScrollState) && viewportH > 0;
    if (showScrollbar) {
        gScrollTrack = (SDL_Rect){
            pane->x + pane->w - trackWidth - trackPadding,
            contentTop,
            trackWidth,
            viewportH
        };
        gScrollThumb = scroll_state_thumb_rect(&gScrollState,
                                               gScrollTrack.x,
                                               gScrollTrack.y,
                                               gScrollTrack.w,
                                               gScrollTrack.h);
        SDL_Color trackColor = gScrollState.track_color;
        SDL_Color thumbColor = gScrollState.thumb_color;
        SDL_SetRenderDrawColor(getRenderContext()->renderer, trackColor.r, trackColor.g, trackColor.b, trackColor.a);
        SDL_RenderFillRect(getRenderContext()->renderer, &gScrollTrack);
        SDL_SetRenderDrawColor(getRenderContext()->renderer, thumbColor.r, thumbColor.g, thumbColor.b, thumbColor.a);
        SDL_RenderFillRect(getRenderContext()->renderer, &gScrollThumb);
    } else {
        gScrollTrack = (SDL_Rect){0};
        gScrollThumb = (SDL_Rect){0};
    }
}

PaneScrollState* assets_get_scroll_state(UIPane* pane) {
    (void)pane;
    return &gScrollState;
}

SDL_Rect assets_get_scroll_track_rect(void) {
    return gScrollTrack;
}

SDL_Rect assets_get_scroll_thumb_rect(void) {
    return gScrollThumb;
}
