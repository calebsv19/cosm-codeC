#ifndef COMMAND_CONTROL_PANEL_H
#define COMMAND_CONTROL_PANEL_H

#include "PaneInfo/pane.h"
#include "InputManager/input_command_enums.h"

void initControlPanelCommandHandler(UIPane* pane);
void handleControlPanelCommand(UIPane* pane, InputCommandMetadata meta);

#endif

