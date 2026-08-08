#include "ide/UI/Trees/tree_row_metrics.h"

bool ui_tree_row_metrics_compute(UITreeRowMetrics* out,
                                 int pane_x,
                                 int tree_padding_x,
                                 int depth,
                                 int indent_width,
                                 int row_y,
                                 int line_height,
                                 int text_width,
                                 int text_height,
                                 int prefix_text_width) {
    if (!out) return false;
    if (line_height < 1) return false;
    if (text_height < 1) text_height = line_height;
    if (text_width < 0) text_width = 0;
    if (prefix_text_width < 0) prefix_text_width = 0;
    if (depth < 0) depth = 0;
    if (indent_width < 0) indent_width = 0;

    int draw_x = pane_x + tree_padding_x + (depth * indent_width);
    int text_y = row_y + ((line_height - text_height) / 2);

    out->draw_x = draw_x;
    out->text_y = text_y;
    out->text_bounds = (SDL_Rect){
        draw_x - 6,
        text_y - 1,
        text_width + 12,
        text_height + 2
    };
    out->prefix_bounds = (SDL_Rect){
        draw_x,
        text_y - 1,
        prefix_text_width + 10,
        text_height + 2
    };
    return true;
}

static bool rect_contains_point(const SDL_Rect* rect, int x, int y) {
    if (!rect || rect->w <= 0 || rect->h <= 0) return false;
    return x >= rect->x && x < (rect->x + rect->w) &&
           y >= rect->y && y < (rect->y + rect->h);
}

bool ui_tree_row_metrics_contains_text(const UITreeRowMetrics* metrics, int x, int y) {
    if (!metrics) return false;
    return rect_contains_point(&metrics->text_bounds, x, y);
}

bool ui_tree_row_metrics_contains_prefix(const UITreeRowMetrics* metrics, int x, int y) {
    if (!metrics) return false;
    return rect_contains_point(&metrics->prefix_bounds, x, y);
}
