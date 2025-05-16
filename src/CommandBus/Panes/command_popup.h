#ifndef COMMAND_POPUP_H
#define COMMAND_POPUP_H

#include "pane.h"
#include "InputManager/input_command_enums.h"
#include "CommandBus/command_metadata.h"

void initPopupCommandHandler(UIPane* pane);
void handlePopupCommand(UIPane* pane, InputCommandMetadata meta);

#endif

