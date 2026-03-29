#include "ide/Panes/ToolPanels/Git/render_tool_git.h"
#include "ide/Panes/ToolPanels/Git/tool_git.h"
#include "ide/Panes/ToolPanels/tool_panel_chrome.h"
#include "ide/Panes/ToolPanels/tool_panel_top_layout.h"
#include "ide/UI/panel_metrics.h"

#include "engine/Render/render_helpers.h"
#include "engine/Render/render_pipeline.h"
#include "engine/Render/render_text_helpers.h"
#include "engine/Render/render_font.h"
#include "ide/UI/panel_control_widgets.h"
#include "ide/UI/panel_text_field.h"
#include "ide/UI/Trees/tree_renderer.h"

#include <SDL2/SDL.h>
#include <string.h>

typedef struct {
    int controlsY;
    int controlsHeight;
    int branchY;
    int statusY;
    int contentTop;
} GitPanelLayout;

static GitPanelLayout git_panel_layout(const UIPane* pane) {
    GitPanelLayout l = {0};
    ToolPanelLayoutDefaults d = tool_panel_layout_defaults();
    l.controlsY = pane->y + d.controls_top - 1;
    l.controlsHeight = d.button_h;

    const int metadataGap = d.row_gap;
    const int metadataLineGap = d.info_line_gap;
    l.branchY = l.controlsY + l.controlsHeight + metadataGap;
    l.statusY = l.branchY + metadataLineGap;
    l.contentTop = git_panel_content_top(pane);

    return l;
}

void renderGitPanel(UIPane* pane) {
    RenderContext* rctx = getRenderContext();
    if (!rctx || !rctx->renderer) return;
    SDL_Renderer* renderer = rctx->renderer;

    GitPanelLayout layout = git_panel_layout(pane);
    tool_panel_render_split_background(renderer, pane, layout.contentTop, 14);

    UIPanelThreeSegmentStripLayout topStrip =
        ui_panel_three_segment_strip_layout(pane->x + 10,
                                            layout.controlsY,
                                            pane->w - 20,
                                            70,
                                            66,
                                            8,
                                            layout.controlsHeight);
    if (topStrip.middle_rect.w < 48) {
        topStrip.middle_rect.w = 48;
        topStrip.trailing_rect.x = topStrip.middle_rect.x + topStrip.middle_rect.w + 8;
    }
    SDL_Rect addAllBtn = topStrip.leading_rect;
    SDL_Rect msgBox = topStrip.middle_rect;
    SDL_Rect commitBtn = topStrip.trailing_rect;
    git_panel_set_top_strip_layout(topStrip);

    ui_panel_compact_button_render(renderer,
                                   &(UIPanelCompactButtonSpec){
                                       .rect = addAllBtn,
                                       .label = "Add All",
                                       .active = false,
                                       .outlined = false,
                                       .use_custom_fill = false,
                                       .use_custom_outline = false,
                                       .tier = CORE_FONT_TEXT_SIZE_CAPTION
                                   });
    ui_panel_compact_button_render(renderer,
                                   &(UIPanelCompactButtonSpec){
                                       .rect = commitBtn,
                                       .label = "Commit",
                                       .active = false,
                                       .outlined = false,
                                       .use_custom_fill = false,
                                       .use_custom_outline = false,
                                       .tier = CORE_FONT_TEXT_SIZE_CAPTION
                                   });

    const char* msg = git_panel_get_message();
    ui_panel_text_field_render(renderer,
                               &(UIPanelTextFieldSpec){
                                   .rect = msgBox,
                                   .text = msg,
                                   .placeholder = "commit message...",
                                   .focused = git_panel_is_message_focused(),
                                   .cursor = git_panel_get_message_cursor(),
                                   .tier = CORE_FONT_TEXT_SIZE_CAPTION
                               });

    TTF_Font* uiFont = getUIFontByTier(CORE_FONT_TEXT_SIZE_CAPTION);
    if (!uiFont) uiFont = getActiveFont();
    int metaLineHeight = uiFont ? TTF_FontHeight(uiFont) : IDE_UI_DENSE_ROW_HEIGHT;
    if (metaLineHeight < IDE_UI_DENSE_ROW_HEIGHT) metaLineHeight = IDE_UI_DENSE_ROW_HEIGHT;

    char branchLine[96];
    const char* branchName = git_panel_branch_name();
    snprintf(branchLine, sizeof(branchLine), "Branch: %s", (branchName && branchName[0]) ? branchName : "unknown");
    SDL_Rect branchClip = { pane->x + 10, layout.branchY, pane->w - 20, metaLineHeight };
    if (branchClip.w < 0) branchClip.w = 0;
    SDL_Color branchColor = {232, 238, 248, 255};
    drawTextUTF8WithFontColorClipped(branchClip.x,
                                     branchClip.y,
                                     branchLine,
                                     uiFont,
                                     branchColor,
                                     false,
                                     &branchClip);

    const char* status = git_panel_get_status_text();
    if (status && status[0]) {
        SDL_Rect statusClip = { pane->x + 10, layout.statusY, pane->w - 20, metaLineHeight };
        if (statusClip.w < 0) statusClip.w = 0;
        SDL_Color statusColor = {213, 220, 233, 255};
        drawTextUTF8WithFontColorClipped(statusClip.x,
                                         statusClip.y,
                                         status,
                                         uiFont,
                                         statusColor,
                                         false,
                                         &statusClip);
    }

    UIPane treePane = {0};
    git_panel_tree_viewport(pane, &treePane);
    renderTreePanelWithScroll(&treePane,
                              git_panel_tree(),
                              git_panel_scroll(),
                              git_panel_scroll_track(),
                              git_panel_scroll_thumb());
}
