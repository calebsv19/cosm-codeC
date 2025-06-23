#ifndef INPUT_ERRORS_H
#define INPUT_ERRORS_H

#include "PaneInfo/pane.h"
#include <SDL2/SDL.h>

void handleErrorsKeyboardInput(UIPane* pane, SDL_Event* event);
void handleErrorsMouseInput(UIPane* pane, SDL_Event* event);
void handleErrorsScrollInput(UIPane* pane, SDL_Event* event);
void handleErrorsHoverInput(UIPane* pane, int x, int y);

#endif

