#ifndef IDE_UI_FLAT_LIST_SELECTION_H
#define IDE_UI_FLAT_LIST_SELECTION_H

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

static inline int ui_flat_list_selection_limit(int capacity, int visibleCount) {
    if (capacity <= 0) return 0;
    if (visibleCount <= 0) return capacity;
    return visibleCount < capacity ? visibleCount : capacity;
}

static inline void ui_flat_list_selection_clear(bool* selected, int capacity) {
    if (!selected || capacity <= 0) return;
    memset(selected, 0, (size_t)capacity * sizeof(*selected));
}

static inline bool ui_flat_list_selection_contains(const bool* selected, int capacity, int idx) {
    if (!selected || idx < 0 || idx >= capacity) return false;
    return selected[idx];
}

static inline bool ui_flat_list_selection_select_single(bool* selected,
                                                        int capacity,
                                                        int visibleCount,
                                                        int idx) {
    int limit = ui_flat_list_selection_limit(capacity, visibleCount);
    if (!selected || idx < 0 || idx >= limit) return false;
    ui_flat_list_selection_clear(selected, capacity);
    selected[idx] = true;
    return true;
}

static inline bool ui_flat_list_selection_toggle(bool* selected,
                                                 int capacity,
                                                 int visibleCount,
                                                 int idx,
                                                 bool additive) {
    int limit = ui_flat_list_selection_limit(capacity, visibleCount);
    if (!selected || idx < 0 || idx >= limit) return false;
    if (!additive) {
        ui_flat_list_selection_clear(selected, capacity);
    }
    selected[idx] = !selected[idx];
    return true;
}

static inline bool ui_flat_list_selection_select_range(bool* selected,
                                                       int capacity,
                                                       int visibleCount,
                                                       int a,
                                                       int b) {
    int limit = ui_flat_list_selection_limit(capacity, visibleCount);
    if (!selected || a < 0 || b < 0 || a >= limit || b >= limit) return false;
    if (a > b) {
        int tmp = a;
        a = b;
        b = tmp;
    }
    ui_flat_list_selection_clear(selected, capacity);
    for (int i = a; i <= b; ++i) {
        selected[i] = true;
    }
    return true;
}

static inline int ui_flat_list_selection_select_all(bool* selected,
                                                    int capacity,
                                                    int visibleCount) {
    int limit = ui_flat_list_selection_limit(capacity, visibleCount);
    ui_flat_list_selection_clear(selected, capacity);
    for (int i = 0; i < limit; ++i) {
        selected[i] = true;
    }
    return limit > 0 ? (limit - 1) : -1;
}

#endif
