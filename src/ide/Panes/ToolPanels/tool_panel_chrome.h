#ifndef TOOL_PANEL_CHROME_H
#define TOOL_PANEL_CHROME_H

#include "ide/Panes/PaneInfo/pane.h"
#include "engine/Render/renderer_backend.h"

typedef struct {
    int controls_y;
    int controls_h;
    int info_start_y;
    int info_line_gap;
    int info_line_count;
    int bottom_padding;
    int min_content_top;
} ToolPanelHeaderMetrics;

typedef struct {
    SDL_Rect header_rect;
    SDL_Rect body_rect;
} ToolPanelSplitLayout;

int tool_panel_compute_content_top(const ToolPanelHeaderMetrics* metrics);
void tool_panel_compute_split_layout(const UIPane* pane, int content_top, ToolPanelSplitLayout* out_layout);
SDL_Color tool_panel_body_color(const UIPane* pane, int darken_amount);
void tool_panel_render_split_background(SDL_Renderer* renderer,
                                        const UIPane* pane,
                                        int content_top,
                                        int darken_amount);

#endif
