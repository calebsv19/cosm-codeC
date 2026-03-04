#ifndef IDE_UI_ROW_ACTIVATION_H
#define IDE_UI_ROW_ACTIVATION_H

#include "ide/UI/interaction_timing.h"

#include <stdbool.h>
#include <stdint.h>

typedef void (*UIRowActivationCallback)(void* user_data);

typedef struct UIRowActivationContext {
    UIDoubleClickTracker* double_click_tracker;
    uintptr_t row_identity;
    unsigned int double_click_ms;
    bool clicked_prefix;
    bool additive_modifier;
    bool range_modifier;
    bool wants_drag_start;
    UIRowActivationCallback on_select_single;
    UIRowActivationCallback on_select_additive;
    UIRowActivationCallback on_select_range;
    UIRowActivationCallback on_prefix;
    UIRowActivationCallback on_activate;
    UIRowActivationCallback on_drag_start;
    void* user_data;
} UIRowActivationContext;

typedef struct UIRowActivationResult {
    bool is_double_click;
    bool did_select;
    bool did_prefix_action;
    bool did_activate;
    bool did_drag_start;
} UIRowActivationResult;

static inline UIRowActivationCallback ui_row_activation_select_callback(const UIRowActivationContext* ctx) {
    if (!ctx) return NULL;
    if (ctx->range_modifier && ctx->on_select_range) {
        return ctx->on_select_range;
    }
    if (ctx->additive_modifier && ctx->on_select_additive) {
        return ctx->on_select_additive;
    }
    return ctx->on_select_single;
}

static inline UIRowActivationResult ui_row_activation_handle_primary(const UIRowActivationContext* ctx) {
    UIRowActivationResult result = {0};
    if (!ctx) return result;

    if (ctx->double_click_tracker) {
        unsigned int threshold = ctx->double_click_ms > 0 ? ctx->double_click_ms : UI_DOUBLE_CLICK_MS_DEFAULT;
        result.is_double_click = ui_double_click_tracker_push(ctx->double_click_tracker,
                                                              ctx->row_identity,
                                                              threshold);
    }

    UIRowActivationCallback select_cb = ui_row_activation_select_callback(ctx);
    if (select_cb) {
        select_cb(ctx->user_data);
        result.did_select = true;
    }

    if (ctx->clicked_prefix && ctx->on_prefix) {
        ctx->on_prefix(ctx->user_data);
        result.did_prefix_action = true;
        return result;
    }

    if (result.is_double_click && ctx->on_activate) {
        ctx->on_activate(ctx->user_data);
        result.did_activate = true;
        return result;
    }

    if (ctx->wants_drag_start && ctx->on_drag_start) {
        ctx->on_drag_start(ctx->user_data);
        result.did_drag_start = true;
    }

    return result;
}

#endif
