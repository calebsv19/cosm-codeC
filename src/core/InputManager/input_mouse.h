#ifndef INPUT_MOUSE_H
#define INPUT_MOUSE_H

#include <SDL2/SDL.h>
#include "ide/Panes/PaneInfo/pane.h"

void handleMouseInput(SDL_Event* event, UIPane** panes, int paneCount);

#endif // INPUT_MOUSE_H

