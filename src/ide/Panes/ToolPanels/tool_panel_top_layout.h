#ifndef TOOL_PANEL_TOP_LAYOUT_H
#define TOOL_PANEL_TOP_LAYOUT_H

#include "ide/Panes/PaneInfo/pane.h"

#include <SDL2/SDL.h>

typedef struct {
    int pad_left;
    int pad_right;
    int controls_top;
    int button_h;
    int row_gap;
    int info_top;
    int info_line_gap;
    int body_darken;
} ToolPanelLayoutDefaults;

typedef struct {
    int x_left;
    int x_right;
    int y;
    int h;
    int gap;
} ToolPanelControlRow;

ToolPanelLayoutDefaults tool_panel_layout_defaults(void);
int tool_panel_content_inset_default(void);
int tool_panel_single_row_content_top(const UIPane* pane);
int tool_panel_info_line_y(const UIPane* pane, int line_index);
int tool_panel_title_text_y(const UIPane* pane);
ToolPanelControlRow tool_panel_control_row_at(const UIPane* pane, int y);
ToolPanelControlRow tool_panel_control_row_with(const UIPane* pane,
                                                int y,
                                                int pad_left,
                                                int pad_right,
                                                int h,
                                                int gap);
SDL_Rect tool_panel_row_take_left(ToolPanelControlRow* row, int w);
SDL_Rect tool_panel_row_take_right(ToolPanelControlRow* row, int w);
SDL_Rect tool_panel_row_take_fill(ToolPanelControlRow* row, int min_w);

#endif
