#include "core/UI/scroll_manager.h"

#include <math.h>

static const PaneScrollConfig kDefaultConfig = {
    .line_height_px = 20.0f,
    .deceleration_px = 400.0f,
    .allow_negative = false,
};

static float clampf(float value, float min, float max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

void scroll_state_init(PaneScrollState* state, const PaneScrollConfig* config) {
    if (!state) return;
    const PaneScrollConfig* cfg = config ? config : &kDefaultConfig;
    state->offset_px = 0.0f;
    state->target_offset_px = 0.0f;
    state->content_height_px = 0.0f;
    state->viewport_height_px = 0.0f;
    state->line_height_px = cfg->line_height_px;
    state->dragging = false;
    state->drag_anchor_offset = 0.0f;
    state->drag_anchor_mouse = 0.0f;
    state->scrolling_enabled = true;
}

void scroll_state_set_viewport(PaneScrollState* state, float viewport_height) {
    if (!state) return;
    state->viewport_height_px = viewport_height;
    scroll_state_clamp(state);
}

void scroll_state_set_content_height(PaneScrollState* state, float content_height) {
    if (!state) return;
    state->content_height_px = content_height;
    scroll_state_clamp(state);
}

void scroll_state_scroll_lines(PaneScrollState* state, float lines) {
    if (!state || !state->scrolling_enabled) return;
    float delta = lines * state->line_height_px;
    scroll_state_scroll_pixels(state, delta);
}

void scroll_state_scroll_pixels(PaneScrollState* state, float pixels) {
    if (!state || !state->scrolling_enabled) return;
    state->target_offset_px -= pixels;
    scroll_state_clamp(state);
    state->offset_px = state->target_offset_px;
}

void scroll_state_begin_drag(PaneScrollState* state, float mouse_y) {
    if (!state || !state->scrolling_enabled) return;
    state->dragging = true;
    state->drag_anchor_offset = state->target_offset_px;
    state->drag_anchor_mouse = mouse_y;
}

void scroll_state_update_drag(PaneScrollState* state, float mouse_y) {
    if (!state || !state->dragging) return;
    float delta = mouse_y - state->drag_anchor_mouse;
    state->target_offset_px = state->drag_anchor_offset - delta;
    scroll_state_clamp(state);
    state->offset_px = state->target_offset_px;
}

void scroll_state_end_drag(PaneScrollState* state) {
    if (!state) return;
    state->dragging = false;
}

void scroll_state_update(PaneScrollState* state, float dt) {
    if (!state) return;
    (void)dt;
    state->offset_px = state->target_offset_px;
}

void scroll_state_clamp(PaneScrollState* state) {
    if (!state) return;
    float maxOffset = state->content_height_px - state->viewport_height_px;
    if (maxOffset < 0.0f) {
        maxOffset = 0.0f;
    }
    state->offset_px = clampf(state->offset_px, 0.0f, maxOffset);
    state->target_offset_px = clampf(state->target_offset_px, 0.0f, maxOffset);
}

float scroll_state_get_offset(PaneScrollState* state) {
    if (!state) return 0.0f;
    return state->offset_px;
}

bool scroll_state_can_scroll(const PaneScrollState* state) {
    if (!state) return false;
    return state->content_height_px > state->viewport_height_px + 0.5f;
}

SDL_Rect scroll_state_thumb_rect(const PaneScrollState* state,
                                 int track_x,
                                 int track_y,
                                 int track_width,
                                 int track_height) {
    SDL_Rect thumb = { track_x, track_y, track_width, 0 };
    if (!state || !scroll_state_can_scroll(state)) {
        thumb.h = track_height;
        return thumb;
    }
    float visibleRatio = state->viewport_height_px / state->content_height_px;
    if (visibleRatio < 0.05f) {
        visibleRatio = 0.05f;
    }
    thumb.h = (int)(visibleRatio * (float)track_height);
    float maxThumbTravel = (float)(track_height - thumb.h);
    float maxOffset = state->content_height_px - state->viewport_height_px;
    float normalized = (maxOffset > 0.0f) ? (state->offset_px / maxOffset) : 0.0f;
    thumb.y = track_y + (int)(normalized * maxThumbTravel);
    return thumb;
}
