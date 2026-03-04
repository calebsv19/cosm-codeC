#ifndef IDE_UI_SCROLL_INPUT_ADAPTER_H
#define IDE_UI_SCROLL_INPUT_ADAPTER_H

#include "ide/UI/scroll_manager.h"

static inline bool ui_scroll_input_consume(PaneScrollState* scroll,
                                           const SDL_Event* event,
                                           const SDL_Rect* track,
                                           SDL_Rect* inout_thumb) {
    if (!scroll || !event) return false;

    SDL_Rect thumb = inout_thumb ? *inout_thumb : (SDL_Rect){0};
    if (scroll_state_handle_mouse_drag(scroll, event, track, &thumb)) {
        if (track && inout_thumb) {
            *inout_thumb = scroll_state_thumb_rect(scroll,
                                                   track->x,
                                                   track->y,
                                                   track->w,
                                                   track->h);
        }
        return true;
    }

    if (event->type == SDL_MOUSEWHEEL && scroll_state_handle_mouse_wheel(scroll, event)) {
        if (track && inout_thumb) {
            *inout_thumb = scroll_state_thumb_rect(scroll,
                                                   track->x,
                                                   track->y,
                                                   track->w,
                                                   track->h);
        }
        return true;
    }

    return false;
}

#endif // IDE_UI_SCROLL_INPUT_ADAPTER_H
