#ifndef IDE_UI_TREES_TREE_ROW_METRICS_H
#define IDE_UI_TREES_TREE_ROW_METRICS_H

#include <SDL2/SDL.h>
#include <stdbool.h>

typedef struct UITreeRowMetrics {
    int draw_x;
    int text_y;
    SDL_Rect text_bounds;
    SDL_Rect prefix_bounds;
} UITreeRowMetrics;

bool ui_tree_row_metrics_compute(UITreeRowMetrics* out,
                                 int pane_x,
                                 int tree_padding_x,
                                 int depth,
                                 int indent_width,
                                 int row_y,
                                 int line_height,
                                 int text_width,
                                 int text_height,
                                 int prefix_text_width);

bool ui_tree_row_metrics_contains_text(const UITreeRowMetrics* metrics, int x, int y);
bool ui_tree_row_metrics_contains_prefix(const UITreeRowMetrics* metrics, int x, int y);

#endif
