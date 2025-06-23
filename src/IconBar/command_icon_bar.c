#include "command_icon_bar.h"
#include "CommandBus/command_metadata.h"
#include "IconBar/icon_bar.h"
#include <stdio.h>

void handleIconBarCommand(UIPane* pane, InputCommandMetadata meta) {
    switch (meta.cmd) {
        case COMMAND_SELECT_ICON_PROJECT_FILES:
            setActiveIcon(ICON_PROJECT_FILES);
            break;

        case COMMAND_SELECT_ICON_LIBRARIES:
            setActiveIcon(ICON_LIBRARIES);
            break;

        case COMMAND_SELECT_ICON_BUILD_OUTPUT:
            setActiveIcon(ICON_BUILD_OUTPUT);
            break;

        case COMMAND_SELECT_ICON_ERRORS:
            setActiveIcon(ICON_ERRORS);
            break;

        case COMMAND_SELECT_ICON_ASSET_MANAGER:
            setActiveIcon(ICON_ASSET_MANAGER);
            break;

        case COMMAND_SELECT_ICON_TASKS:
            setActiveIcon(ICON_TASKS);
            break;

        case COMMAND_SELECT_ICON_VERSION_CONTROL:
            setActiveIcon(ICON_VERSION_CONTROL);
            break;

        default:
            printf("[IconBarCommand] Unknown command: %d\n", meta.cmd);
            break;
    }
}


void initIconBarCommandHandler(UIPane* pane) {
    if (pane) {
        pane->handleCommand = handleIconBarCommand;
    }
}

