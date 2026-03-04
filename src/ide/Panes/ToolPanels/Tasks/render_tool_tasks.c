#include "render_tool_tasks.h"
#include "engine/Render/render_pipeline.h"
#include "engine/Render/render_helpers.h"
#include "engine/Render/render_text_helpers.h"
#include "engine/Render/render_font.h"
#include "ide/Panes/ToolPanels/tool_panel_chrome.h"
#include "ide/Panes/ToolPanels/tool_panel_top_layout.h"

#include "ide/Panes/ToolPanels/Tasks/tool_tasks.h"
#include "ide/UI/panel_control_widgets.h"
#include "ide/UI/row_surface.h"
#include "ide/UI/shared_theme_font_adapter.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <string.h>

static const PaneScrollConfig g_taskScrollCfg = {
    .line_height_px = (float)TASK_LINE_HEIGHT,
    .deceleration_px = 0.0f,
    .allow_negative = false
};

typedef struct TaskTopControlSpec {
    TaskTopControlId id;
    const char* symbol;
    const char* label;
} TaskTopControlSpec;

static const TaskTopControlSpec s_taskTopControls[] = {
    { TASK_TOP_CONTROL_ADD, "+", "Add Task" },
    { TASK_TOP_CONTROL_REMOVE, "-", "Remove Task" },
};

static TTF_Font* task_row_font(void) {
    TTF_Font* font = getUIFontByTier(CORE_FONT_TEXT_SIZE_CAPTION);
    return font ? font : getActiveFont();
}

static SDL_Color task_row_color(void) {
    IDEThemePalette palette = {0};
    SDL_Color color = {230, 230, 230, 255};
    if (ide_shared_theme_resolve_palette(&palette)) {
        color = palette.text_primary;
    }
    return color;
}

static UIRowSurfaceRenderSpec task_row_surface_spec(const TaskNode* node) {
    UIRowSurfaceRenderSpec spec = {0};
    if (!node) return spec;
    spec.draw_selection_fill = node->isSelected;
    spec.draw_selection_outline = node->isSelected && !node->isHovered;
    spec.draw_hover_outline = node->isHovered;
    return spec;
}

static int count_visible_task_rows(TaskNode* node) {
    if (!node) return 0;
    int count = 1;
    if (node->isExpanded) {
        for (int i = 0; i < node->childCount; ++i) {
            count += count_visible_task_rows(node->children[i]);
        }
    }
    return count;
}

static void renderTaskTreeRecursive(TaskNode* node,
                                    int x,
                                    int* y,
                                    int contentTop,
                                    int maxY,
                                    const SDL_Rect* clipRect,
                                    TTF_Font* rowFont,
                                    SDL_Color rowColor) {
    if (!node) return;
    if (*y > maxY) return;

    RenderContext* ctx = getRenderContext();
    SDL_Renderer* renderer = ctx->renderer;

    UITaskRowLayout row = {0};
    task_build_row_layout(node, NULL, x, *y, &row);
    UIRowSurfaceLayout surface = ui_row_surface_layout_from_rect(row.fullRowRect);
    UIRowSurfaceLayout visibleSurface = {0};
    bool rowVisible = ui_row_surface_clip(&surface, clipRect, &visibleSurface);

    if (row.drawY + TASK_LINE_HEIGHT > contentTop && row.drawY < maxY && rowVisible) {
        UIRowSurfaceRenderSpec rowSpec = task_row_surface_spec(node);
        ui_row_surface_render(renderer, &visibleSurface, &rowSpec);

        if (row.hasExpandIcon) {
            drawTextUTF8WithFontColorClipped(row.drawX,
                                             row.drawY,
                                             node->isExpanded ? "[-]" : "[+]",
                                             rowFont,
                                             rowColor,
                                             false,
                                             clipRect);
        }

        drawTextUTF8WithFontColorClipped(row.drawX + TASK_EXPAND_WIDTH,
                                         row.drawY,
                                         node->completed ? "[x]" : "[ ]",
                                         rowFont,
                                         rowColor,
                                         false,
                                         clipRect);

        if (node == editingTask) {
            drawTextUTF8WithFontColorClipped(row.labelX,
                                             row.drawY,
                                             taskEditingBuffer,
                                             rowFont,
                                             rowColor,
                                             false,
                                             clipRect);
        } else {
            drawTextUTF8WithFontColorClipped(row.labelX,
                                             row.drawY,
                                             node->label,
                                             rowFont,
                                             rowColor,
                                             false,
                                             clipRect);
        }
    }

    *y += TASK_LINE_HEIGHT;

    if (node->isExpanded) {
        for (int i = 0; i < node->childCount; i++) {
            renderTaskTreeRecursive(node->children[i],
                                    x,
                                    y,
                                    contentTop,
                                    maxY,
                                    clipRect,
                                    rowFont,
                                    rowColor);
        }
    }
}


void renderTasksPanel(UIPane* pane) {
    RenderContext* ctx = getRenderContext();
    SDL_Renderer* renderer = ctx ? ctx->renderer : NULL;
    ToolPanelLayoutDefaults d = tool_panel_layout_defaults();
    int x = pane->x + d.pad_left;
    int y = pane->y + d.controls_top + 8;
    int maxY = pane->y + pane->h;
    const int trackWidth = 6;
    const int trackPadding = 4;

    const int iconBtnSize = 24;
    const int topControlCount = (int)(sizeof(s_taskTopControls) / sizeof(s_taskTopControls[0]));
    UIPanelVerticalButtonStackLayout topControls =
        ui_panel_vertical_button_stack_layout(x, y, iconBtnSize, iconBtnSize, TASK_BUTTON_SPACING, topControlCount);
    UIPanelTaggedRectList* controlHits = task_panel_control_hits();
    ui_panel_tagged_rect_list_reset(controlHits);

    for (int i = 0; i < topControls.count; ++i) {
        SDL_Rect buttonRect = topControls.button_rects[i];
        ui_panel_compact_button_render(renderer,
                                       &(UIPanelCompactButtonSpec){
                                           .rect = buttonRect,
                                           .label = s_taskTopControls[i].symbol,
                                           .active = false,
                                           .outlined = false,
                                           .use_custom_fill = false,
                                           .use_custom_outline = false,
                                           .tier = CORE_FONT_TEXT_SIZE_CAPTION
                                       });
        (void)ui_panel_tagged_rect_list_add(controlHits, s_taskTopControls[i].id, buttonRect);
        drawText(buttonRect.x + buttonRect.w + 6, buttonRect.y + 4, s_taskTopControls[i].label);
    }

    y += (topControls.count * iconBtnSize) +
         ((topControls.count > 0 ? topControls.count - 1 : 0) * TASK_BUTTON_SPACING);

    int contentTop = y;
    PaneScrollState* scroll = task_panel_scroll_state();
    if (!*task_panel_scroll_initialized_ptr()) {
        scroll_state_init(scroll, &g_taskScrollCfg);
        *task_panel_scroll_initialized_ptr() = true;
    }
    if (renderer) {
        tool_panel_render_split_background(renderer, pane, contentTop, 14);
    }

    int viewportH = maxY - contentTop;
    if (viewportH < 0) viewportH = 0;
    scroll_state_set_viewport(scroll, (float)viewportH);

    int rowCount = 0;
    for (int i = 0; i < taskRootCount; ++i) {
        rowCount += count_visible_task_rows(taskRoots[i]);
    }
    float contentHeight = (float)TASK_TREE_TOP_GAP + (float)(rowCount * TASK_LINE_HEIGHT);
    scroll_state_set_content_height(scroll,
                                    scroll_state_top_anchor_content_height(scroll, contentHeight));

    SDL_Rect clipRect = {
        pane->x,
        contentTop,
        pane->w - (trackWidth + trackPadding),
        viewportH
    };
    if (clipRect.w < 0) clipRect.w = 0;

    y = contentTop + TASK_TREE_TOP_GAP - (int)scroll_state_get_offset(scroll);
    TTF_Font* rowFont = task_row_font();
    SDL_Color rowColor = task_row_color();
    pushClipRect(&clipRect);
    for (int i = 0; i < taskRootCount; i++) {
        renderTaskTreeRecursive(taskRoots[i],
                                x,
                                &y,
                                contentTop,
                                maxY,
                                &clipRect,
                                rowFont,
                                rowColor);
    }
    popClipRect();

    if (scroll_state_can_scroll(scroll) && viewportH > 0 && renderer) {
        SDL_Rect track = {
            pane->x + pane->w - trackWidth,
            contentTop,
            trackWidth,
            viewportH
        };
        SDL_Rect thumb = scroll_state_thumb_rect(scroll,
                                                 track.x,
                                                 track.y,
                                                 track.w,
                                                 track.h);
        SDL_SetRenderDrawColor(renderer, scroll->track_color.r, scroll->track_color.g,
                               scroll->track_color.b, scroll->track_color.a);
        SDL_RenderFillRect(renderer, &track);
        SDL_SetRenderDrawColor(renderer, scroll->thumb_color.r, scroll->thumb_color.g,
                               scroll->thumb_color.b, scroll->thumb_color.a);
        SDL_RenderFillRect(renderer, &thumb);
        task_panel_set_scroll_rects(track, thumb);
    } else {
        task_panel_set_scroll_rects((SDL_Rect){0}, (SDL_Rect){0});
    }
}
