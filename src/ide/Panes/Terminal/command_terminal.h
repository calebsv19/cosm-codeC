#ifndef COMMAND_TERMINAL_H
#define COMMAND_TERMINAL_H

#include "ide/Panes/PaneInfo/pane.h"
#include "core/InputManager/input_command_enums.h"

void initTerminalCommandHandler(UIPane* pane);
void handleTerminalCommand(UIPane* pane, InputCommandMetadata meta);

#endif

