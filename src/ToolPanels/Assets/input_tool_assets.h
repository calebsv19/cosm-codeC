#ifndef INPUT_ASSETS_H
#define INPUT_ASSETS_H

#include "PaneInfo/pane.h"
#include <SDL2/SDL.h>

void handleAssetsKeyboardInput(UIPane* pane, SDL_Event* event);
void handleAssetsMouseInput(UIPane* pane, SDL_Event* event);
void handleAssetsScrollInput(UIPane* pane, SDL_Event* event);
void handleAssetsHoverInput(UIPane* pane, int x, int y);

#endif

