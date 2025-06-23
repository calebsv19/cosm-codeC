#include "ToolPanels/Errors/command_tool_errors.h"
#include "CommandBus/command_metadata.h"
#include "ToolPanels/Errors/tool_errors.h"
#include <stdio.h>

void handleErrorsCommand(UIPane* pane, InputCommandMetadata meta) {
    (void)pane;

    switch (meta.cmd) {
        case COMMAND_JUMP_TO_FIRST_ERROR:
            printf("[ErrorPanelCommand] Jumping to first error (not yet implemented)\n");
            // Future: jumpToFirstError();
            break;

        case COMMAND_CLEAR_ERROR_LIST:
            printf("[ErrorPanelCommand] Clearing error list (not yet implemented)\n");
            // Future: clearErrorList();
            break;

        default:
            printf("[ErrorPanelCommand] Unknown command: %d\n", meta.cmd);
            break;
    }
}

