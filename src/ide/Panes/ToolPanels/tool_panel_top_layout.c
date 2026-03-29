#include "ide/Panes/ToolPanels/tool_panel_top_layout.h"
#include "ide/UI/panel_metrics.h"
#include "engine/Render/render_font.h"

#include <SDL2/SDL_ttf.h>

enum {
    TOOL_PANEL_TITLE_TOP_PAD = 6,
    TOOL_PANEL_TITLE_BOTTOM_GAP = 8
};

static int tool_panel_font_height(CoreFontTextSizeTier tier, int fallback_min) {
    TTF_Font* font = getUIFontByTier(tier);
    int h = 0;
    if (!font) {
        font = getActiveFont();
    }
    if (font) {
        h = TTF_FontHeight(font);
    }
    if (h < fallback_min) {
        h = fallback_min;
    }
    return h;
}

ToolPanelLayoutDefaults tool_panel_layout_defaults(void) {
    const int dense_row_h = IDE_UI_DENSE_ROW_HEIGHT;
    const int title_h = tool_panel_font_height(CORE_FONT_TEXT_SIZE_TITLE, dense_row_h);
    const int header_h = tool_panel_font_height(CORE_FONT_TEXT_SIZE_HEADER, dense_row_h);
    const int banner_h = (title_h > header_h) ? title_h : header_h;
    ToolPanelLayoutDefaults d = {
        .pad_left = 12,
        .pad_right = 12,
        .controls_top = TOOL_PANEL_TITLE_TOP_PAD + banner_h + TOOL_PANEL_TITLE_BOTTOM_GAP,
        .button_h = dense_row_h + 6,
        .row_gap = 6,
        .info_top = 8,
        .info_line_gap = dense_row_h,
        .body_darken = 14
    };
    return d;
}

int tool_panel_content_inset_default(void) {
    return 6;
}

int tool_panel_single_row_content_top(const UIPane* pane) {
    if (!pane) return 0;
    ToolPanelLayoutDefaults d = tool_panel_layout_defaults();
    return pane->y + d.controls_top + d.button_h + d.row_gap;
}

int tool_panel_info_line_y(const UIPane* pane, int line_index) {
    if (!pane) return 0;
    ToolPanelLayoutDefaults d = tool_panel_layout_defaults();
    if (line_index < 0) line_index = 0;
    return pane->y + d.info_top + (line_index * d.info_line_gap);
}

int tool_panel_title_text_y(const UIPane* pane) {
    if (!pane) return 0;
    return pane->y + TOOL_PANEL_TITLE_TOP_PAD;
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
