#ifndef COMMAND_EDITOR_H
#define COMMAND_EDITOR_H

#include "PaneInfo/pane.h"
#include "InputManager/input_command_enums.h"

// Main handler for commands dispatched to an editor pane
void handleEditorCommand(UIPane* pane, InputCommandMetadata meta);

// Register this as the editor's handler during pane creation
void initEditorCommandHandler(UIPane* pane);

#endif // COMMAND_EDITOR_H

