#ifndef INPUT_LIBRARIES_H
#define INPUT_LIBRARIES_H

#include "PaneInfo/pane.h"
#include <SDL2/SDL.h>

void handleLibrariesKeyboardInput(UIPane* pane, SDL_Event* event);
void handleLibrariesMouseInput(UIPane* pane, SDL_Event* event);
void handleLibrariesScrollInput(UIPane* pane, SDL_Event* event);
void handleLibrariesHoverInput(UIPane* pane, int x, int y);

#endif // INPUT_LIBRARIES_H

