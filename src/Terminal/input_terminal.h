#ifndef INPUT_TERMINAL_H
#define INPUT_TERMINAL_H

#include "PaneInfo/pane.h"
#include <SDL2/SDL.h>

// Future input hooks
void handleTerminalKeyboardInput(UIPane* pane, SDL_Event* event);
void handleTerminalMouseInput(UIPane* pane, SDL_Event* event);
void handleTerminalScrollInput(UIPane* pane, SDL_Event* event);
void handleTerminalHoverInput(UIPane* pane, int x, int y);

// Exported handler
extern UIPaneInputHandler terminalInputHandler;

#endif // INPUT_TERMINAL_H

