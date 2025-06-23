#ifndef INPUT_TOOL_PANEL_H
#define INPUT_TOOL_PANEL_H

#include "PaneInfo/pane.h"
#include <SDL2/SDL.h>

// Core pane input handler functions
void handleToolPanelKeyboardInput(UIPane* pane, SDL_Event* event);
void handleToolPanelMouseInput(UIPane* pane, SDL_Event* event);
void handleToolPanelScrollInput(UIPane* pane, SDL_Event* event);
void handleToolPanelHoverInput(UIPane* pane, int x, int y);

// Exported handler struct
extern UIPaneInputHandler toolPanelInputHandler;

#endif // INPUT_TOOL_PANEL_H

