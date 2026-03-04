#ifndef IDE_UI_INTERACTION_TIMING_H
#define IDE_UI_INTERACTION_TIMING_H

#include "core/LoopTime/loop_time.h"

#include <stdbool.h>
#include <stdint.h>

enum { UI_DOUBLE_CLICK_MS_DEFAULT = 400u };

typedef struct {
    uint32_t last_tick_ms;
    uintptr_t last_key;
    bool has_last;
} UIDoubleClickTracker;

static inline void ui_double_click_tracker_reset(UIDoubleClickTracker* tracker) {
    if (!tracker) return;
    tracker->last_tick_ms = 0;
    tracker->last_key = 0;
    tracker->has_last = false;
}

static inline bool ui_double_click_tracker_push(UIDoubleClickTracker* tracker,
                                                uintptr_t key,
                                                uint32_t threshold_ms) {
    uint32_t now = loop_time_now_ms32();
    bool is_double_click = false;

    if (tracker && tracker->has_last && tracker->last_key == key) {
        uint32_t elapsed_ms = now - tracker->last_tick_ms;
        is_double_click = elapsed_ms <= threshold_ms;
    }

    if (tracker) {
        tracker->last_tick_ms = now;
        tracker->last_key = key;
        tracker->has_last = true;
    }

    return is_double_click;
}

#endif // IDE_UI_INTERACTION_TIMING_H
