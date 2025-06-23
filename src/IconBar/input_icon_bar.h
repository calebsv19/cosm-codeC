#ifndef INPUT_ICON_BAR_H
#define INPUT_ICON_BAR_H

#include "PaneInfo/pane.h"
#include <SDL2/SDL.h>

// Pane-level input handlers
void handleIconBarKeyboardInput(UIPane* pane, SDL_Event* event);
void handleIconBarMouseInput(UIPane* pane, SDL_Event* event);
void handleIconBarScrollInput(UIPane* pane, SDL_Event* event);
void handleIconBarHoverInput(UIPane* pane, int x, int y);

// Input handler instance
extern UIPaneInputHandler iconBarInputHandler;

#endif // INPUT_ICON_BAR_H

