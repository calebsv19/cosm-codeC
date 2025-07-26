#pragma once

#include "ide/Panes/PaneInfo/pane.h"
#include "core/CommandBus/command_metadata.h"


void initToolPanelCommandHandler(UIPane* pane);

void handleToolPanelCommand(UIPane* pane, InputCommandMetadata meta);

