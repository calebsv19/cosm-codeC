#pragma once

#include "pane.h"
#include "CommandBus/command_metadata.h"


void initToolPanelCommandHandler(UIPane* pane);

void handleToolPanelCommand(UIPane* pane, InputCommandMetadata meta);

