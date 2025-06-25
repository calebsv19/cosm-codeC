#ifndef INPUT_KEYBOARD_H
#define INPUT_KEYBOARD_H

#include <SDL2/SDL.h>
#include "ide/Panes/PaneInfo/pane.h"
#include "ide/UI/ui_state.h"

void handleKeyboardInput(SDL_Event* event,
                         UIPane** panes, int* paneCount, bool* running);

#endif // INPUT_KEYBOARD_H

