#include "ide/UI/scroll_manager.h"

#include <math.h>

static const PaneScrollConfig kDefaultConfig = {
    .line_height_px = 20.0f,
    .deceleration_px = 400.0f,
    .allow_negative = false,
};

static SDL_Color g_default_track_color = {45, 45, 45, 150};
static SDL_Color g_default_thumb_color = {100, 100, 100, 220};

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
    state->dragging_thumb = false;
    state->thumb_drag_offset = 0.0f;
    state->track_color = g_default_track_color;
    state->thumb_color = g_default_thumb_color;
}

void scroll_manager_set_default_colors(SDL_Color track, SDL_Color thumb) {
    g_default_track_color = track;
    g_default_thumb_color = thumb;
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

void scroll_state_begin_thumb_drag(PaneScrollState* state,
                                   float mouse_y,
                                   const SDL_Rect* track,
                                   const SDL_Rect* thumb) {
    if (!state || !track || !thumb) return;
    if (!scroll_state_can_scroll(state)) return;
    state->dragging_thumb = true;
    state->thumb_drag_offset = mouse_y - thumb->y;
}

void scroll_state_update_thumb_drag(PaneScrollState* state,
                                    float mouse_y,
                                    const SDL_Rect* track,
                                    const SDL_Rect* thumb) {
    if (!state || !state->dragging_thumb || !track || !thumb) return;
    float maxTravel = (float)(track->h - thumb->h);
    if (maxTravel <= 0.0f) return;

    float newThumbTop = mouse_y - state->thumb_drag_offset;
    if (newThumbTop < track->y) newThumbTop = (float)track->y;
    if (newThumbTop > track->y + maxTravel) newThumbTop = track->y + maxTravel;

    float normalized = (newThumbTop - track->y) / maxTravel;
    float maxOffset = state->content_height_px - state->viewport_height_px;
    if (maxOffset < 0.0f) maxOffset = 0.0f;
    state->target_offset_px = normalized * maxOffset;
    scroll_state_clamp(state);
    state->offset_px = state->target_offset_px;
}

void scroll_state_end_drag(PaneScrollState* state) {
    if (!state) return;
    state->dragging = false;
    state->dragging_thumb = false;
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

bool scroll_state_is_dragging_thumb(const PaneScrollState* state) {
    return state ? state->dragging_thumb : false;
}

bool scroll_state_is_dragging(const PaneScrollState* state) {
    return state ? (state->dragging || state->dragging_thumb) : false;
}

bool scroll_state_handle_mouse_wheel(PaneScrollState* state, const SDL_Event* event) {
    if (!state || !event || event->type != SDL_MOUSEWHEEL) return false;
    if (!scroll_state_can_scroll(state)) return false;
    float lines = (event->wheel.preciseY != 0.0f) ? (float)event->wheel.preciseY
                                                  : (float)event->wheel.y;
    if (event->wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
        lines = -lines;
    }
    if (lines == 0.0f) return false;
    scroll_state_scroll_lines(state, lines);
    return true;
}

bool scroll_state_handle_mouse_drag(PaneScrollState* state,
                                    const SDL_Event* event,
                                    const SDL_Rect* track,
                                    const SDL_Rect* thumb) {
    if (!state || !event) return false;
    if (!scroll_state_can_scroll(state)) return false;

    switch (event->type) {
        case SDL_MOUSEBUTTONDOWN:
            if (event->button.button == SDL_BUTTON_LEFT && thumb && track) {
                if (event->button.x >= thumb->x && event->button.x < thumb->x + thumb->w &&
                    event->button.y >= thumb->y && event->button.y < thumb->y + thumb->h) {
                    scroll_state_begin_thumb_drag(state, (float)event->button.y, track, thumb);
                    return true;
                }
            }
            break;
        case SDL_MOUSEBUTTONUP:
            if (event->button.button == SDL_BUTTON_LEFT && state->dragging_thumb) {
                scroll_state_end_drag(state);
                return true;
            }
            break;
        case SDL_MOUSEMOTION:
            if (state->dragging_thumb && track && thumb) {
                scroll_state_update_thumb_drag(state, (float)event->motion.y, track, thumb);
                return true;
            }
            break;
        default:
            break;
    }
    return false;
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
