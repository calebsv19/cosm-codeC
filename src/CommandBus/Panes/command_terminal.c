#include "command_terminal.h"
#include "CommandBus/command_metadata.h"
#include "Terminal/terminal.h"
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

        case COMMAND_RUN_EXECUTABLE:
            // TODO: Implement actual executable runner
            printToTerminal("[Run] Command execution not implemented.");
            break;

        default:
            printf("[TerminalCommand] Unhandled command: %d\n", meta.cmd);
            break;
    }
}

