#ifndef INPUT_BUILD_OUTPUT_H
#define INPUT_BUILD_OUTPUT_H

#include "PaneInfo/pane.h"
#include <SDL2/SDL.h>

void handleBuildOutputKeyboardInput(UIPane* pane, SDL_Event* event);
void handleBuildOutputMouseInput(UIPane* pane, SDL_Event* event);
void handleBuildOutputScrollInput(UIPane* pane, SDL_Event* event);
void handleBuildOutputHoverInput(UIPane* pane, int x, int y);

#endif

