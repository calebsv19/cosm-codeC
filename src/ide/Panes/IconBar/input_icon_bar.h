#ifndef INPUT_ICON_BAR_H
#define INPUT_ICON_BAR_H
#include "kit_ui.h"

#include "ide/Panes/PaneInfo/pane.h"
#include <SDL2/SDL.h>
KitUiButtonState ide_icon_bar_button_state(UIPane* pane, int index);

// Pane-level input handlers
void handleIconBarKeyboardInput(UIPane* pane, SDL_Event* event);
void handleIconBarMouseInput(UIPane* pane, SDL_Event* event);
void handleIconBarScrollInput(UIPane* pane, SDL_Event* event);
void handleIconBarHoverInput(UIPane* pane, int x, int y);

// Input handler instance
extern UIPaneInputHandler iconBarInputHandler;

#endif // INPUT_ICON_BAR_H

