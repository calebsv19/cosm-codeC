#ifndef CORE_UI_SCROLL_MANAGER_H
#define CORE_UI_SCROLL_MANAGER_H

#include <SDL2/SDL.h>
#include <stdbool.h>

typedef struct PaneScrollState {
    float offset_px;
    float target_offset_px;
    float content_height_px;
    float viewport_height_px;
    float line_height_px;
    bool dragging;
    float drag_anchor_offset;
    float drag_anchor_mouse;
    bool scrolling_enabled;
    bool dragging_thumb;
    float thumb_drag_offset;
    SDL_Color track_color;
    SDL_Color thumb_color;
} PaneScrollState;

typedef struct PaneScrollConfig {
    float line_height_px;
    float deceleration_px;
    bool allow_negative;
} PaneScrollConfig;

void scroll_state_init(PaneScrollState* state, const PaneScrollConfig* config);
void scroll_state_set_viewport(PaneScrollState* state, float viewport_height);
void scroll_state_set_content_height(PaneScrollState* state, float content_height);
float scroll_state_top_anchor_content_height(const PaneScrollState* state, float content_height);
void scroll_state_scroll_lines(PaneScrollState* state, float lines);
void scroll_state_scroll_pixels(PaneScrollState* state, float pixels);
void scroll_state_begin_drag(PaneScrollState* state, float mouse_y);
void scroll_state_update_drag(PaneScrollState* state, float mouse_y);
void scroll_state_begin_thumb_drag(PaneScrollState* state, float mouse_y, const SDL_Rect* track, const SDL_Rect* thumb);
void scroll_state_update_thumb_drag(PaneScrollState* state, float mouse_y, const SDL_Rect* track, const SDL_Rect* thumb);
void scroll_state_end_drag(PaneScrollState* state);
void scroll_state_update(PaneScrollState* state, float dt);
void scroll_state_clamp(PaneScrollState* state);
float scroll_state_get_offset(PaneScrollState* state);
SDL_Rect scroll_state_thumb_rect(const PaneScrollState* state,
                                 int track_x,
                                 int track_y,
                                 int track_width,
                                 int track_height);
bool scroll_state_can_scroll(const PaneScrollState* state);
bool scroll_state_handle_mouse_wheel(PaneScrollState* state, const SDL_Event* event);
bool scroll_state_handle_mouse_drag(PaneScrollState* state,
                                    const SDL_Event* event,
                                    const SDL_Rect* track,
                                    const SDL_Rect* thumb);
bool scroll_state_is_dragging_thumb(const PaneScrollState* state);
bool scroll_state_is_dragging(const PaneScrollState* state);
void scroll_manager_set_default_colors(SDL_Color track, SDL_Color thumb);

#endif /* CORE_UI_SCROLL_MANAGER_H */
