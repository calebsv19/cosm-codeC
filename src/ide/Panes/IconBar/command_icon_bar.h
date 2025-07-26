#ifndef COMMAND_ICON_BAR_H
#define COMMAND_ICON_BAR_H

#include "ide/Panes/PaneInfo/pane.h"
#include "core/InputManager/input_command_enums.h"

void handleIconBarCommand(UIPane* pane, InputCommandMetadata meta);
void initIconBarCommandHandler(UIPane* pane);

#endif
	
