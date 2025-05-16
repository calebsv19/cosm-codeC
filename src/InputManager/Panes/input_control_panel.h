#ifndef INPUT_CONTROL_PANEL_H
#define INPUT_CONTROL_PANEL_H

#include "pane.h"
#include <SDL2/SDL.h>

// Future input hooks
void handleControlPanelKeyboardInput(UIPane* pane, SDL_Event* event);
void handleControlPanelMouseInput(UIPane* pane, SDL_Event* event);
void handleControlPanelScrollInput(UIPane* pane, SDL_Event* event);
void handleControlPanelHoverInput(UIPane* pane, int x, int y);

// Exported handler
extern UIPaneInputHandler controlPanelInputHandler;

#endif // INPUT_CONTROL_PANEL_H

