#include "command_tool_build_output.h"
#include "CommandBus/command_metadata.h"
#include "ToolPanels/tool_build_output.h"
#include <stdio.h>

void handleBuildOutputCommand(UIPane* pane, InputCommandMetadata meta) {
    (void)pane;

    switch (meta.cmd) {
        case COMMAND_CLEAR_BUILD_OUTPUT:
            printf("[BuildOutputCommand] Clearing build output (not implemented)\n");
            // Future: clearBuildLog();
            break;

        case COMMAND_SCROLL_BUILD_OUTPUT_TOP:
            printf("[BuildOutputCommand] Scroll to top (not implemented)\n");
            // Future: scrollBuildLogToTop();
            break;

        default:
            printf("[BuildOutputCommand] Unknown command: %d\n", meta.cmd);
            break;
    }
}

