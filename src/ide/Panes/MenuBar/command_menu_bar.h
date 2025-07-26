#ifndef COMMAND_MENU_BAR_H
#define COMMAND_MENU_BAR_H

#include "ide/Panes/PaneInfo/pane.h"
#include "core/InputManager/input_command_enums.h"

void handleMenuBarCommand(UIPane* pane, InputCommandMetadata meta);
void initMenuBarCommandHandler(UIPane* pane);

#endif

