#ifndef COMMAND_POPUP_H
#define COMMAND_POPUP_H

#include "ide/Panes/PaneInfo/pane.h"
#include "core/InputManager/input_command_enums.h"
#include "core/CommandBus/command_metadata.h"

void initPopupCommandHandler(UIPane* pane);
void handlePopupCommand(UIPane* pane, InputCommandMetadata meta);

#endif

