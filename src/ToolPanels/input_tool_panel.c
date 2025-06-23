
#include "input_tool_panel.h"
#include "IconBar/icon_bar.h" // for getActiveIcon()


// Subpanel Input Handlers
#include "ToolPanels/Project/input_tool_project.h"
#include "ToolPanels/Libraries/input_tool_libraries.h"
#include "ToolPanels/Tasks/input_tool_tasks.h"
#include "ToolPanels/BuildOutput/input_tool_build_output.h"
#include "ToolPanels/Errors/input_tool_errors.h"
#include "ToolPanels/Assets/input_tool_assets.h"
#include "ToolPanels/Git/input_tool_git.h"

#include "ToolPanels/command_tool_panel.h"



// ==== Keyboard ====
void handleToolPanelKeyboardInput(UIPane* pane, SDL_Event* event) {
    IconTool current = getActiveIcon();

    switch (current) {
        case ICON_PROJECT_FILES:     handleProjectFilesKeyboardInput(pane, event); break;
        case ICON_LIBRARIES:         handleLibrariesKeyboardInput(pane, event); break;
        case ICON_TASKS:             handleTasksKeyboardInput(pane, event); break;
        case ICON_BUILD_OUTPUT:      handleBuildOutputKeyboardInput(pane, event); break;
        case ICON_ERRORS:            handleErrorsKeyboardInput(pane, event); break;
        case ICON_ASSET_MANAGER:     handleAssetsKeyboardInput(pane, event); break;
        case ICON_VERSION_CONTROL:   handleGitKeyboardInput(pane, event); break;
        default: break;
    }
}


// ==== Mouse ====
void handleToolPanelMouseInput(UIPane* pane, SDL_Event* event) {
    IconTool current = getActiveIcon();

    switch (current) {
        case ICON_PROJECT_FILES:     handleProjectFilesMouseInput(pane, event); break;
        case ICON_LIBRARIES:         handleLibrariesMouseInput(pane, event); break;
        case ICON_TASKS:             handleTasksMouseInput(pane, event); break;
        case ICON_BUILD_OUTPUT:      handleBuildOutputMouseInput(pane, event); break;
        case ICON_ERRORS:            handleErrorsMouseInput(pane, event); break;
        case ICON_ASSET_MANAGER:     handleAssetsMouseInput(pane, event); break;
        case ICON_VERSION_CONTROL:   handleGitMouseInput(pane, event); break;
        default: break;
    }
}


// ==== Scroll ====
void handleToolPanelScrollInput(UIPane* pane, SDL_Event* event) {
    IconTool current = getActiveIcon();

    switch (current) {
        case ICON_PROJECT_FILES:     handleProjectFilesScrollInput(pane, event); break;
        case ICON_LIBRARIES:         handleLibrariesScrollInput(pane, event); break;
        case ICON_TASKS:             handleTasksScrollInput(pane, event); break;
        case ICON_BUILD_OUTPUT:      handleBuildOutputScrollInput(pane, event); break;
        case ICON_ERRORS:            handleErrorsScrollInput(pane, event); break;
        case ICON_ASSET_MANAGER:     handleAssetsScrollInput(pane, event); break;
        case ICON_VERSION_CONTROL:   handleGitScrollInput(pane, event); break;
        default: break;
    }
}

// ==== Hover ====
void handleToolPanelHoverInput(UIPane* pane, int x, int y) {
    IconTool current = getActiveIcon();

    switch (current) {
        case ICON_PROJECT_FILES:     handleProjectFilesHoverInput(pane, x, y); break;
        case ICON_LIBRARIES:         handleLibrariesHoverInput(pane, x, y); break;
        case ICON_TASKS:             handleTasksHoverInput(pane, x, y); break;
        case ICON_BUILD_OUTPUT:      handleBuildOutputHoverInput(pane, x, y); break;
        case ICON_ERRORS:            handleErrorsHoverInput(pane, x, y); break;
        case ICON_ASSET_MANAGER:     handleAssetsHoverInput(pane, x, y); break;
        case ICON_VERSION_CONTROL:   handleGitHoverInput(pane, x, y); break;
        default: break;
    }
}


// ==== Handler Struct Export ====
UIPaneInputHandler toolPanelInputHandler = {
    .onCommand = handleToolPanelCommand,
    .onKeyboard = handleToolPanelKeyboardInput,
    .onMouse = handleToolPanelMouseInput,
    .onScroll = handleToolPanelScrollInput,
    .onHover = handleToolPanelHoverInput,
};

