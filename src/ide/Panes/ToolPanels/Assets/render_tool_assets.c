#include "ide/Panes/ToolPanels/Assets/render_tool_assets.h"
#include "engine/Render/render_helpers.h"
#include "engine/Render/render_pipeline.h"
#include "engine/Render/render_text_helpers.h"
#include "ide/Panes/ToolPanels/Assets/tool_assets.h"
#include "ide/Panes/ToolPanels/tool_panel_chrome.h"
#include "ide/Panes/ToolPanels/tool_panel_top_layout.h"
#include "ide/UI/row_surface.h"
#include "ide/UI/scroll_manager.h"
#include "engine/Render/render_font.h"
#include "ide/UI/shared_theme_font_adapter.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
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
    PaneScrollState* scroll = assets_get_scroll_state(pane);
    const int headerHeight = ASSET_PANEL_HEADER_HEIGHT;
    const int lineHeight = ASSET_PANEL_ROW_HEIGHT;
    if (!scroll->line_height_px && !scroll->viewport_height_px && !scroll->content_height_px) {
        PaneScrollConfig cfg = {
            .line_height_px = (float)lineHeight,
            .deceleration_px = 0.0f,
            .allow_negative = false
        };
        scroll_state_init(scroll, &cfg);
    }
    scroll->line_height_px = (float)lineHeight;
    ToolPanelLayoutDefaults d = tool_panel_layout_defaults();
    const int paddingX = d.pad_left;
    const int controlsY = pane->y + d.controls_top;
    const int trackWidth = 6;
    const int trackPadding = 4;

    UIPanelTaggedRectList* controlHits = assets_get_control_hits();
    ui_panel_tagged_rect_list_reset(controlHits);
    const UIPanelCompactButtonRowItem controlItems[] = {
        { ASSET_TOP_CONTROL_OPEN_ALL, "Open All", false, false },
        { ASSET_TOP_CONTROL_CLOSE_ALL, "Close All", false, false }
    };
    ui_panel_compact_button_row_render(getRenderContext()->renderer,
                                       pane->x + d.pad_left,
                                       controlsY,
                                       84,
                                       d.button_h,
                                       d.row_gap,
                                       controlItems,
                                       (int)(sizeof(controlItems) / sizeof(controlItems[0])),
                                       CORE_FONT_TEXT_SIZE_CAPTION,
                                       controlHits);

    int contentTop = tool_panel_single_row_content_top(pane);
    ToolPanelSplitLayout split = {0};
    tool_panel_compute_split_layout(pane, contentTop, &split);
    int viewportH = split.body_rect.h;
    scroll_state_set_viewport(scroll, (float)viewportH);

    tool_panel_render_split_background(getRenderContext()->renderer, pane, contentTop, d.body_darken);

    AssetFlatRef refs[1024];
    int count = assets_flatten(refs, 1024);

    float contentHeight = 0.0f;
    for (int i = 0; i < count; ++i) {
        contentHeight += (refs[i].isHeader ? (float)headerHeight : (float)lineHeight);
    }
    scroll_state_set_content_height(scroll,
                                    scroll_state_top_anchor_content_height(scroll, contentHeight));

    float offset = scroll_state_get_offset(scroll);

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
    int mouseX = 0;
    int mouseY = 0;
    SDL_GetMouseState(&mouseX, &mouseY);

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
                UIRowSurfaceLayout rowSurface = ui_row_surface_layout_from_rect(textBox);
                UIRowSurfaceLayout visibleSurface = {0};
                if (!ui_row_surface_clip(&rowSurface, &clipRect, &visibleSurface)) {
                    y += h;
                    continue;
                }

                bool isSelected = assets_is_selected(i);
                bool isHovered = ui_row_surface_contains(&visibleSurface, mouseX, mouseY);
                ui_row_surface_render(getRenderContext()->renderer,
                                      &visibleSurface,
                                      &(UIRowSurfaceRenderSpec){
                                          .draw_selection_fill = isSelected,
                                          .draw_selection_outline = isSelected,
                                          .draw_hover_outline = isHovered
                                      });

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

    bool showScrollbar = scroll_state_can_scroll(scroll) && viewportH > 0;
    if (showScrollbar) {
        SDL_Rect track = (SDL_Rect){
            split.body_rect.x + split.body_rect.w - trackWidth,
            split.body_rect.y,
            trackWidth,
            viewportH
        };
        SDL_Rect thumb = scroll_state_thumb_rect(scroll,
                                                 track.x,
                                                 track.y,
                                                 track.w,
                                                 track.h);
        SDL_Color trackColor = scroll->track_color;
        SDL_Color thumbColor = scroll->thumb_color;
        SDL_SetRenderDrawColor(getRenderContext()->renderer, trackColor.r, trackColor.g, trackColor.b, trackColor.a);
        SDL_RenderFillRect(getRenderContext()->renderer, &track);
        SDL_SetRenderDrawColor(getRenderContext()->renderer, thumbColor.r, thumbColor.g, thumbColor.b, thumbColor.a);
        SDL_RenderFillRect(getRenderContext()->renderer, &thumb);
        assets_set_scroll_rects(track, thumb);
    } else {
        assets_set_scroll_rects((SDL_Rect){0}, (SDL_Rect){0});
    }
}
