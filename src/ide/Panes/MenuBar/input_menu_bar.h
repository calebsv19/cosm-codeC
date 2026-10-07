#ifndef INPUT_MENU_BAR_H
#define INPUT_MENU_BAR_H

#include "kit_ui.h"
#include "ide/Panes/PaneInfo/pane.h"
#include <SDL2/SDL.h>

// Pane-level input functions
void handleMenuBarKeyboardInput(UIPane* pane, SDL_Event* event);
void handleMenuBarMouseInput(UIPane* pane, SDL_Event* event);
void handleMenuBarScrollInput(UIPane* pane, SDL_Event* event);
void handleMenuBarHoverInput(UIPane* pane, int x, int y);

// Input handler reference
extern UIPaneInputHandler menuBarInputHandler;

void ide_menu_bar_cancel(void);
KitUiButtonState ide_menu_bar_button_state(UIPane* pane, int index);

#endif // INPUT_MENU_BAR_H

