#ifndef IDE_UI_FLAT_LIST_INTERACTION_H
#define IDE_UI_FLAT_LIST_INTERACTION_H

#include <stdbool.h>

typedef struct {
    bool active;
    int anchor;
} UIFlatListDragState;

typedef void (*UIFlatListClearSelectionFn)(void* context);
typedef void (*UIFlatListSelectRangeFn)(int anchorIndex, int hitIndex, void* context);

static inline void ui_flat_list_drag_state_begin(UIFlatListDragState* state, int anchor) {
    if (!state) return;
    state->active = (anchor >= 0);
    state->anchor = (anchor >= 0) ? anchor : -1;
}

static inline void ui_flat_list_drag_state_reset(UIFlatListDragState* state) {
    ui_flat_list_drag_state_begin(state, -1);
}

static inline bool ui_flat_list_drag_state_prepare_hit(UIFlatListDragState* state,
                                                       int hit,
                                                       UIFlatListClearSelectionFn clearSelection,
                                                       void* context) {
    if (hit >= 0) return true;
    if (clearSelection) {
        clearSelection(context);
    }
    ui_flat_list_drag_state_reset(state);
    return false;
}

static inline bool ui_flat_list_drag_state_apply_range(const UIFlatListDragState* state,
                                                       int hit,
                                                       UIFlatListSelectRangeFn selectRange,
                                                       void* context) {
    if (!state || !state->active || state->anchor < 0 || hit < 0 || !selectRange) return false;
    selectRange(state->anchor, hit, context);
    return true;
}

#endif
