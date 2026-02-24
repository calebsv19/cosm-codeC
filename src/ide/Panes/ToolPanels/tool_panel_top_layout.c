#include "ide/Panes/ToolPanels/tool_panel_top_layout.h"

ToolPanelLayoutDefaults tool_panel_layout_defaults(void) {
    ToolPanelLayoutDefaults d = {
        .pad_left = 12,
        .pad_right = 12,
        .controls_top = 24,
        .button_h = 20,
        .row_gap = 6,
        .info_top = 8,
        .info_line_gap = 14,
        .body_darken = 14
    };
    return d;
}

int tool_panel_content_inset_default(void) {
    return 6;
}

int tool_panel_info_line_y(const UIPane* pane, int line_index) {
    if (!pane) return 0;
    ToolPanelLayoutDefaults d = tool_panel_layout_defaults();
    if (line_index < 0) line_index = 0;
    return pane->y + d.info_top + (line_index * d.info_line_gap);
}

ToolPanelControlRow tool_panel_control_row_with(const UIPane* pane,
                                                int y,
                                                int pad_left,
                                                int pad_right,
                                                int h,
                                                int gap) {
    ToolPanelControlRow row = {0};
    if (!pane) return row;

    int left = pane->x + pad_left;
    int right = pane->x + pane->w - pad_right;
    if (right < left) right = left;

    row.x_left = left;
    row.x_right = right;
    row.y = y;
    row.h = h;
    row.gap = gap;
    return row;
}

ToolPanelControlRow tool_panel_control_row_at(const UIPane* pane, int y) {
    ToolPanelLayoutDefaults d = tool_panel_layout_defaults();
    return tool_panel_control_row_with(pane, y, d.pad_left, d.pad_right, d.button_h, d.row_gap);
}

SDL_Rect tool_panel_row_take_left(ToolPanelControlRow* row, int w) {
    SDL_Rect rect = {0};
    if (!row || w <= 0) return rect;
    if (row->x_right < row->x_left) row->x_right = row->x_left;

    int avail = row->x_right - row->x_left;
    if (w > avail) w = avail;
    if (w < 0) w = 0;

    rect = (SDL_Rect){ row->x_left, row->y, w, row->h };
    row->x_left += w + row->gap;
    if (row->x_left > row->x_right) row->x_left = row->x_right;
    return rect;
}

SDL_Rect tool_panel_row_take_right(ToolPanelControlRow* row, int w) {
    SDL_Rect rect = {0};
    if (!row || w <= 0) return rect;
    if (row->x_right < row->x_left) row->x_right = row->x_left;

    int avail = row->x_right - row->x_left;
    if (w > avail) w = avail;
    if (w < 0) w = 0;

    rect = (SDL_Rect){ row->x_right - w, row->y, w, row->h };
    row->x_right -= (w + row->gap);
    if (row->x_right < row->x_left) row->x_right = row->x_left;
    return rect;
}

SDL_Rect tool_panel_row_take_fill(ToolPanelControlRow* row, int min_w) {
    SDL_Rect rect = {0};
    if (!row) return rect;
    if (row->x_right < row->x_left) row->x_right = row->x_left;

    int avail = row->x_right - row->x_left;
    if (avail < min_w) avail = min_w;
    if (avail < 0) avail = 0;

    rect = (SDL_Rect){ row->x_left, row->y, avail, row->h };
    row->x_left = row->x_right;
    return rect;
}
