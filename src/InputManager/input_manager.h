#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H

#include <SDL2/SDL.h>
#include "../GlobalInfo/core_state.h"
#include "../pane.h"
#include "../UI/ui_state.h"
#include "../UI/resize.h"

// Top-level input handler
void handleInput(SDL_Event* event,
                 UIPane** panes, int paneCount,
                 ResizeZone* zones, int zoneCount,
                 int* paneCountRef, bool* running);


#endif // INPUT_MANAGER_H

