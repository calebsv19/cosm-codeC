#include "command_tool_panel.h"

#include "ToolPanels/Project/command_tool_project.h"
#include "ToolPanels/Libraries/command_tool_libraries.h"
#include "ToolPanels/BuildOutput/command_tool_build_output.h"
#include "ToolPanels/Errors/command_tool_errors.h"
#include "ToolPanels/Assets/command_tool_assets.h"
#include "ToolPanels/Tasks/command_tool_tasks.h"
#include "ToolPanels/Git/command_tool_git.h"


#include "CommandBus/command_metadata.h"
#include "IconBar/icon_bar.h"

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

