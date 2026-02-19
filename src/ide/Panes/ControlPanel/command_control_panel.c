#include "command_control_panel.h"
#include "core/CommandBus/command_metadata.h"
#include "ide/Panes/ControlPanel/control_panel.h"
#include <stdio.h>

void initControlPanelCommandHandler(UIPane* pane) {
    if (pane) {
        pane->handleCommand = handleControlPanelCommand;
    }
}

void handleControlPanelCommand(UIPane* pane, InputCommandMetadata meta) {
    (void)pane;

    switch (meta.cmd) {
        case COMMAND_TOGGLE_LIVE_PARSE:
            toggleLiveParse();
            printf("[ControlPanel] Toggled Live Parse\n");
            break;

        case COMMAND_TOGGLE_SHOW_ERRORS:
            toggleShowInlineErrors();
            printf("[ControlPanel] Toggled Inline Errors\n");
            break;

        case COMMAND_TOGGLE_SHOW_MACROS:
            toggleShowMacros();
            printf("[ControlPanel] Toggled Show Macros\n");
            break;

        default:
            printf("[ControlPanel] Unknown command: %d\n", meta.cmd);
            break;
    }
}
