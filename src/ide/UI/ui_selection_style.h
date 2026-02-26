#ifndef UI_SELECTION_STYLE_H
#define UI_SELECTION_STYLE_H

#include <SDL2/SDL.h>

static inline SDL_Color ui_selection_fill_color(void) {
    return (SDL_Color){64, 84, 112, 92};
}

static inline SDL_Color ui_selection_outline_color(void) {
    return (SDL_Color){108, 136, 176, 176};
}

#endif /* UI_SELECTION_STYLE_H */
