#include "ide/Panes/ToolPanels/Assets/render_tool_assets.h"
#include "engine/Render/render_helpers.h"
#include "engine/Render/render_pipeline.h"
#include "ide/Panes/ToolPanels/Assets/tool_assets.h"
#include "ide/Panes/ToolPanels/tool_panel_chrome.h"
#include "ide/Panes/ToolPanels/tool_panel_top_layout.h"
#include "ide/UI/scroll_manager.h"
#include "ide/UI/ui_selection_style.h"

#include <SDL2/SDL.h>
#include <stdio.h>

static PaneScrollState gScrollState;
static bool gScrollInit = false;
static SDL_Rect gScrollTrack = {0};
static SDL_Rect gScrollThumb = {0};
static SDL_Rect gOpenAllRect = {0};
static SDL_Rect gCloseAllRect = {0};
static const PaneScrollConfig gScrollCfg = { .line_height_px = 18.0f, .deceleration_px = 0.0f, .allow_negative = false };

void renderAssetManagerPanel(UIPane* pane) {
    if (!gScrollInit) {
        scroll_state_init(&gScrollState, &gScrollCfg);
        gScrollInit = true;
    }

    const int headerHeight = 18;
    const int lineHeight = 16;
    ToolPanelLayoutDefaults d = tool_panel_layout_defaults();
    const int contentInset = tool_panel_content_inset_default();
    const int paddingX = d.pad_left;
    const int controlsY = pane->y + d.controls_top;
    const int paddingY = d.controls_top + d.button_h + d.row_gap;
    const int trackWidth = 6;
    const int trackPadding = 4;

    ToolPanelControlRow controls = tool_panel_control_row_at(pane, controlsY);
    gOpenAllRect = tool_panel_row_take_left(&controls, 84);
    gCloseAllRect = tool_panel_row_take_left(&controls, 84);
    renderButton(pane, gOpenAllRect, "Open All");
    renderButton(pane, gCloseAllRect, "Close All");

    int contentTop = pane->y + paddingY;
    int viewportH = pane->h - (contentTop - pane->y);
    if (viewportH < 0) viewportH = 0;
    scroll_state_set_viewport(&gScrollState, (float)viewportH);

    tool_panel_render_split_background(getRenderContext()->renderer, pane, contentTop, 14);

    AssetFlatRef refs[1024];
    int count = assets_flatten(refs, 1024);

    float contentHeight = (float)contentInset;
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
    int y = (contentTop + contentInset) - (int)offset;
    int maxY = contentTop + viewportH;

    if (count == 0) {
        drawText(x, y, "(No assets found)");
    } else {
        for (int i = 0; i < count; ++i) {
            int h = refs[i].isHeader ? headerHeight : lineHeight;
            if (y + h > contentTop && y < maxY) {
                if (assets_is_selected(i)) {
                    SDL_Rect rect = { x - 6, y - 2, clipRect.w - paddingX + 4, h };
                    SDL_Color sel = ui_selection_fill_color();
                    SDL_SetRenderDrawColor(getRenderContext()->renderer, sel.r, sel.g, sel.b, sel.a);
                    SDL_RenderFillRect(getRenderContext()->renderer, &rect);
                }

                if (refs[i].isHeader) {
                    const AssetCategoryList* list = &assets_get_catalog()->categories[refs[i].category];
                    char buf[128];
                    snprintf(buf, sizeof(buf), "%s (%d)%s",
                             (const char*[]){"Images","Audio","Data","Docs","Other"}[refs[i].category],
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

SDL_Rect assets_get_open_all_rect(void) {
    return gOpenAllRect;
}

SDL_Rect assets_get_close_all_rect(void) {
    return gCloseAllRect;
}
