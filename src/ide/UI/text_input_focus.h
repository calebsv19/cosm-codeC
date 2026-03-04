#ifndef IDE_UI_TEXT_INPUT_FOCUS_H
#define IDE_UI_TEXT_INPUT_FOCUS_H

#include <stdbool.h>

static inline bool ui_text_input_focus_set(bool* focusedState, bool focused) {
    if (!focusedState || *focusedState == focused) return false;
    *focusedState = focused;
    return true;
}

#endif
