#ifndef INPUT_MENU_BAR_H
#define INPUT_MENU_BAR_H

#include "pane.h"
#include <SDL2/SDL.h>

// Pane-level input functions
void handleMenuBarKeyboardInput(UIPane* pane, SDL_Event* event);
void handleMenuBarMouseInput(UIPane* pane, SDL_Event* event);
void handleMenuBarScrollInput(UIPane* pane, SDL_Event* event);
void handleMenuBarHoverInput(UIPane* pane, int x, int y);

// Input handler reference
extern UIPaneInputHandler menuBarInputHandler;

#endif // INPUT_MENU_BAR_H

