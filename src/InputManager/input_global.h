#ifndef INPUT_GLOBAL_H
#define INPUT_GLOBAL_H

#include <SDL2/SDL.h>
#include "PaneInfo/pane.h"

void handleWindowGlobalEvents(SDL_Event* event,
                              UIPane** panes, int* paneCountRef,
                              bool* running);

void handleWindowEvent(SDL_Event* event);

#endif // INPUT_GLOBAL_H

