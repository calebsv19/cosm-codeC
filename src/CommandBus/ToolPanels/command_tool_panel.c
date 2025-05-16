#include "command_tool_panel.h"

#include "command_tool_project.h"
#include "command_tool_libraries.h"
#include "command_tool_build_output.h"
#include "command_tool_errors.h"
#include "command_tool_assets.h"
#include "command_tool_tasks.h"
#include "command_tool_git.h"


#include "CommandBus/command_metadata.h"
#include "ToolPanels/icon_bar.h"

#include <stdio.h>



void initToolPanelCommandHandler(UIPane* pane) {
    if (pane) {
        pane->handleCommand = handleToolPanelCommand;
    }
}



void handleToolPanelCommand(UIPane* pane, InputCommandMetadata meta) {
    IconTool current = getActiveIcon();

    switch (current) {
        case ICON_PROJECT_FILES:
            handleProjectFilesCommand(pane, meta);
            break;

        case ICON_LIBRARIES:
            handleLibrariesCommand(pane, meta);
            break;

        case ICON_TASKS:
            handleTasksCommand(pane, meta);
            break;

        case ICON_BUILD_OUTPUT:
            handleBuildOutputCommand(pane, meta);
            break;

        case ICON_ERRORS:
            handleErrorsCommand(pane, meta);
            break;

        case ICON_ASSET_MANAGER:
            handleAssetsCommand(pane, meta);
            break;

        case ICON_VERSION_CONTROL:
            handleGitCommand(pane, meta);
            break;

        default:
            printf("[ToolPanelCommand] Unknown icon tool: %d\n", current);
            break;
    }
}

