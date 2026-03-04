#ifndef IDE_UI_INPUT_MODIFIERS_H
#define IDE_UI_INPUT_MODIFIERS_H

#include <SDL2/SDL.h>
#include <stdbool.h>

static inline bool ui_input_has_primary_accel(Uint16 modifiers) {
    return (modifiers & (KMOD_CTRL | KMOD_GUI)) != 0;
}

static inline bool ui_input_is_additive_selection(Uint16 modifiers) {
    return (modifiers & (KMOD_CTRL | KMOD_GUI | KMOD_SHIFT)) != 0;
}

static inline bool ui_input_has_shift(Uint16 modifiers) {
    return (modifiers & KMOD_SHIFT) != 0;
}

#endif
