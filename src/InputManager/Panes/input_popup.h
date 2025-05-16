#ifndef INPUT_POPUP_H
#define INPUT_POPUP_H

#include "pane.h"
#include <SDL2/SDL.h>

// Future popup interaction handlers
void handlePopupKeyboardInput(UIPane* pane, SDL_Event* event);
void handlePopupMouseInput(UIPane* pane, SDL_Event* event);
void handlePopupScrollInput(UIPane* pane, SDL_Event* event);
void handlePopupHoverInput(UIPane* pane, int x, int y);

// Exported handler struct
extern UIPaneInputHandler popupInputHandler;

#endif // INPUT_POPUP_H

