#ifndef INPUT_HOVER_H
#define INPUT_HOVER_H

#include <SDL2/SDL.h>
#include "ide/Panes/PaneInfo/pane.h"

// Called from input_manager on mouse motion
void handleHoverUpdate(SDL_Event* event, UIPane** panes, int paneCount);

#endif // INPUT_HOVER_H


