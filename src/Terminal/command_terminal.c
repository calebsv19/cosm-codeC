#include "command_terminal.h"
#include "Terminal/terminal.h"

#include "Build/run_build.h"

#include "GlobalInfo/project.h"
#include "CommandBus/command_metadata.h"
#include <stdio.h>

void initTerminalCommandHandler(UIPane* pane) {
    if (pane) {
        pane->handleCommand = handleTerminalCommand;
    }
}

void handleTerminalCommand(UIPane* pane, InputCommandMetadata meta) {
    (void)pane;

    switch (meta.cmd) {
        case COMMAND_CLEAR_TERMINAL:
            clearTerminal();
            break;

        case COMMAND_RUN_EXECUTABLE: {
	    char exePath[1024];
	    snprintf(exePath, sizeof(exePath),
	             "%s/include/BuildOutputs/last_build", projectRootPath);
	    runExecutableAndStreamOutput(exePath);
	    break;
	}

        default:
            printf("[TerminalCommand] Unhandled command: %d\n", meta.cmd);
            break;
    }
}

