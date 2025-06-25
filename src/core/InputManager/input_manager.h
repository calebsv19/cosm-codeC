#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H

#include <SDL2/SDL.h>
#include "app/GlobalInfo/core_state.h"
#include "ide/Panes/PaneInfo/pane.h"
#include "ide/UI/ui_state.h"
#include "ide/UI/resize.h"

// Top-level input handler
void handleInput(SDL_Event* event,
                 UIPane** panes, int paneCount,
                 ResizeZone* zones, int zoneCount,
                 int* paneCountRef, bool* running);


#endif // INPUT_MANAGER_H

