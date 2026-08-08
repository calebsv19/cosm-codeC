#include "ide/UI/Trees/tree_row_metrics.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    UITreeRowMetrics metrics;
    assert(ui_tree_row_metrics_compute(&metrics,
                                       100,
                                       12,
                                       2,
                                       14,
                                       40,
                                       18,
                                       80,
                                       14,
                                       28));

    assert(metrics.draw_x == 140);
    assert(metrics.text_y == 42);
    assert(metrics.text_bounds.x == 134);
    assert(metrics.text_bounds.y == 41);
    assert(metrics.text_bounds.w == 92);
    assert(metrics.text_bounds.h == 16);

    assert(ui_tree_row_metrics_contains_text(&metrics, 134, 41));
    assert(ui_tree_row_metrics_contains_text(&metrics, 225, 56));
    assert(!ui_tree_row_metrics_contains_text(&metrics, 133, 41));
    assert(!ui_tree_row_metrics_contains_text(&metrics, 226, 56));
    assert(!ui_tree_row_metrics_contains_text(&metrics, 150, 40));

    assert(ui_tree_row_metrics_contains_prefix(&metrics, 140, 41));
    assert(ui_tree_row_metrics_contains_prefix(&metrics, 177, 56));
    assert(!ui_tree_row_metrics_contains_prefix(&metrics, 139, 41));
    assert(!ui_tree_row_metrics_contains_prefix(&metrics, 178, 56));

    assert(!ui_tree_row_metrics_compute(NULL, 0, 0, 0, 0, 0, 18, 0, 0, 0));
    assert(!ui_tree_row_metrics_compute(&metrics, 0, 0, 0, 0, 0, 0, 0, 0, 0));

    printf("tree_row_metrics_test: ok\n");
    return 0;
}
