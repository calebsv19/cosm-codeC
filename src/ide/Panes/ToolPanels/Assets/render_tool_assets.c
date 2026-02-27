#include "ide/Panes/ToolPanels/Assets/render_tool_assets.h"
#include "engine/Render/render_helpers.h"
#include "engine/Render/render_pipeline.h"
#include "engine/Render/render_text_helpers.h"
#include "ide/Panes/ToolPanels/Assets/tool_assets.h"
#include "ide/Panes/ToolPanels/tool_panel_chrome.h"
#include "ide/Panes/ToolPanels/tool_panel_top_layout.h"
#include "ide/UI/scroll_manager.h"
#include "ide/UI/ui_selection_style.h"
#include "engine/Render/render_font.h"
#include "ide/UI/shared_theme_font_adapter.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>

static PaneScrollState gScrollState;
static bool gScrollInit = false;
static SDL_Rect gScrollTrack = {0};
static SDL_Rect gScrollThumb = {0};
static SDL_Rect gOpenAllRect = {0};
static SDL_Rect gCloseAllRect = {0};
static const PaneScrollConfig gScrollCfg = { .line_height_px = 18.0f, .deceleration_px = 0.0f, .allow_negative = false };

static TTF_Font* asset_row_font(void) {
    TTF_Font* font = getUIFontByTier(CORE_FONT_TEXT_SIZE_CAPTION);
    return font ? font : getActiveFont();
}

static SDL_Color asset_row_color(bool isHeader) {
    IDEThemePalette palette = {0};
    SDL_Color primary = {230, 230, 230, 255};
    SDL_Color muted = {170, 170, 170, 255};
    if (ide_shared_theme_resolve_palette(&palette)) {
        primary = palette.text_primary;
        muted = palette.text_muted;
    }
    return isHeader ? primary : muted;
}

void renderAssetManagerPanel(UIPane* pane) {
    if (!gScrollInit) {
        scroll_state_init(&gScrollState, &gScrollCfg);
        gScrollInit = true;
    }

    const int headerHeight = 18;
    const int lineHeight = 16;
    ToolPanelLayoutDefaults d = tool_panel_layout_defaults();
    const int paddingX = d.pad_left;
    const int controlsY = pane->y + d.controls_top;
    const int trackWidth = 6;
    const int trackPadding = 4;

    ToolPanelControlRow controls = tool_panel_control_row_at(pane, controlsY);
    gOpenAllRect = tool_panel_row_take_left(&controls, 84);
    gCloseAllRect = tool_panel_row_take_left(&controls, 84);
    renderButton(pane, gOpenAllRect, "Open All");
    renderButton(pane, gCloseAllRect, "Close All");

    int contentTop = tool_panel_single_row_content_top(pane);
    ToolPanelSplitLayout split = {0};
    tool_panel_compute_split_layout(pane, contentTop, &split);
    int viewportH = split.body_rect.h;
    scroll_state_set_viewport(&gScrollState, (float)viewportH);

    tool_panel_render_split_background(getRenderContext()->renderer, pane, contentTop, d.body_darken);

    AssetFlatRef refs[1024];
    int count = assets_flatten(refs, 1024);

    float contentHeight = 0.0f;
    for (int i = 0; i < count; ++i) {
        contentHeight += (refs[i].isHeader ? (float)headerHeight : (float)lineHeight);
    }
    scroll_state_set_content_height(&gScrollState, contentHeight);

    float offset = scroll_state_get_offset(&gScrollState);

    SDL_Rect clipRect = {
        split.body_rect.x,
        split.body_rect.y,
        split.body_rect.w - (trackWidth + trackPadding),
        viewportH
    };
    if (clipRect.w < 0) clipRect.w = 0;
    pushClipRect(&clipRect);

    int x = split.body_rect.x + (paddingX - 1);
    int y = split.body_rect.y - (int)offset;
    int maxY = split.body_rect.y + viewportH;
    TTF_Font* rowFont = asset_row_font();
    int fontHeight = rowFont ? TTF_FontHeight(rowFont) : lineHeight;
    if (fontHeight < 1) fontHeight = lineHeight;

    if (count == 0) {
        drawTextUTF8WithFontColorClipped(x,
                                         y,
                                         "(No assets found)",
                                         rowFont,
                                         asset_row_color(true),
                                         false,
                                         &clipRect);
    } else {
        for (int i = 0; i < count; ++i) {
            int h = refs[i].isHeader ? headerHeight : lineHeight;
            if (y + h > contentTop && y < maxY) {
                int drawX = x;
                const char* text = NULL;
                char buf[128];
                char moreBuf[64];
                if (refs[i].isHeader) {
                    const AssetCategoryList* list = &assets_get_catalog()->categories[refs[i].category];
                    snprintf(buf, sizeof(buf), "%s (%d)%s",
                             (const char*[]){"Images","Audio","Data","Docs","Other"}[refs[i].category],
                             list->count,
                             list->collapsed ? " [collapsed]" : "");
                    text = buf;
                } else if (refs[i].isMoreLine) {
                    const AssetCategoryList* list = &assets_get_catalog()->categories[refs[i].category];
                    int remaining = list->count - ASSET_RENDER_LIMIT_PER_BUCKET;
                    snprintf(moreBuf, sizeof(moreBuf), "... and %d more", remaining > 0 ? remaining : 0);
                    text = moreBuf;
                    drawX = x + 12;
                } else if (refs[i].entry) {
                    text = refs[i].entry->relPath ? refs[i].entry->relPath : refs[i].entry->name;
                    drawX = x + 12;
                }

                if (!text) {
                    y += h;
                    continue;
                }

                int textWidth = getTextWidthWithFont(text, rowFont);
                int textY = y + ((h - fontHeight) / 2);
                SDL_Rect textBox = {
                    drawX - 6,
                    textY - 1,
                    textWidth + 12,
                    fontHeight + 2
                };
                SDL_Rect visibleBox = {0};
                if (!SDL_IntersectRect(&textBox, &clipRect, &visibleBox)) {
                    y += h;
                    continue;
                }

                if (assets_is_selected(i)) {
                    SDL_Color sel = ui_selection_fill_color();
                    SDL_SetRenderDrawColor(getRenderContext()->renderer, sel.r, sel.g, sel.b, sel.a);
                    SDL_RenderFillRect(getRenderContext()->renderer, &visibleBox);
                }

                drawTextUTF8WithFontColorClipped(drawX,
                                                 textY,
                                                 text,
                                                 rowFont,
                                                 asset_row_color(refs[i].isHeader),
                                                 false,
                                                 &clipRect);
            }
            y += h;
        }
    }

    popClipRect();

    bool showScrollbar = scroll_state_can_scroll(&gScrollState) && viewportH > 0;
    if (showScrollbar) {
        gScrollTrack = (SDL_Rect){
            split.body_rect.x + split.body_rect.w - trackWidth,
            split.body_rect.y,
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

SDL_Rect assets_get_open_all_rect(void) {
    return gOpenAllRect;
}

SDL_Rect assets_get_close_all_rect(void) {
    return gCloseAllRect;
}
