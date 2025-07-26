#include "command_tool_panel.h"

#include "ide/Panes/ToolPanels/Project/command_tool_project.h"
#include "ide/Panes/ToolPanels/Libraries/command_tool_libraries.h"
#include "ide/Panes/ToolPanels/BuildOutput/command_tool_build_output.h"
#include "ide/Panes/ToolPanels/Errors/command_tool_errors.h"
#include "ide/Panes/ToolPanels/Assets/command_tool_assets.h"
#include "ide/Panes/ToolPanels/Tasks/command_tool_tasks.h"
#include "ide/Panes/ToolPanels/Git/command_tool_git.h"


#include "core/CommandBus/command_metadata.h"
#include "ide/Panes/IconBar/icon_bar.h"

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
	    handleGitCommand(pane, (InputCommandMetadata){ .cmd = COMMAND_REFRESH_GIT_STATUS });
            handleGitCommand(pane, meta);
            break;

        default:
            printf("[ToolPanelCommand] Unknown icon tool: %d\n", current);
            break;
    }
}

