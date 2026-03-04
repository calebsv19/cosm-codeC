#include "ide/Panes/ToolPanels/tool_panel_adapter.h"

#include "ide/Panes/IconBar/icon_bar.h"

#include "ide/Panes/ToolPanels/Project/render_tool_project.h"
#include "ide/Panes/ToolPanels/Project/input_tool_project.h"
#include "ide/Panes/ToolPanels/Project/command_tool_project.h"

#include "ide/Panes/ToolPanels/Libraries/render_tool_libraries.h"
#include "ide/Panes/ToolPanels/Libraries/input_tool_libraries.h"
#include "ide/Panes/ToolPanels/Libraries/command_tool_libraries.h"

#include "ide/Panes/ToolPanels/BuildOutput/render_tool_build_output.h"
#include "ide/Panes/ToolPanels/BuildOutput/input_tool_build_output.h"
#include "ide/Panes/ToolPanels/BuildOutput/command_tool_build_output.h"

#include "ide/Panes/ToolPanels/Errors/render_tool_errors.h"
#include "ide/Panes/ToolPanels/Errors/input_tool_errors.h"
#include "ide/Panes/ToolPanels/Errors/command_tool_errors.h"

#include "ide/Panes/ToolPanels/Assets/render_tool_assets.h"
#include "ide/Panes/ToolPanels/Assets/input_tool_assets.h"
#include "ide/Panes/ToolPanels/Assets/command_tool_assets.h"

#include "ide/Panes/ToolPanels/Tasks/render_tool_tasks.h"
#include "ide/Panes/ToolPanels/Tasks/input_tool_tasks.h"
#include "ide/Panes/ToolPanels/Tasks/command_tool_tasks.h"

#include "ide/Panes/ToolPanels/Git/render_tool_git.h"
#include "ide/Panes/ToolPanels/Git/tool_git.h"
#include "ide/Panes/ToolPanels/Git/input_tool_git.h"
#include "ide/Panes/ToolPanels/Git/command_tool_git.h"

#include "ide/UI/ui_state.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    void* slots[TOOL_PANEL_STATE_SLOT_COUNT];
    void (*destroy_slots[TOOL_PANEL_STATE_SLOT_COUNT])(void*);
} ToolPanelControllerState;

static UIPane* s_toolPanelDispatchPane = NULL;

static void tool_panel_destroy_controller_state(void* ptr) {
    ToolPanelControllerState* state = (ToolPanelControllerState*)ptr;
    if (!state) return;

    for (int i = 0; i < TOOL_PANEL_STATE_SLOT_COUNT; ++i) {
        if (state->slots[i] && state->destroy_slots[i]) {
            state->destroy_slots[i](state->slots[i]);
        }
        state->slots[i] = NULL;
        state->destroy_slots[i] = NULL;
    }

    free(state);
}

void tool_panel_attach_controller(UIPane* pane) {
    if (!pane) return;
    if (pane->controllerState && pane->destroyControllerState == tool_panel_destroy_controller_state) {
        return;
    }

    ToolPanelControllerState* state = (ToolPanelControllerState*)calloc(1, sizeof(*state));
    if (!state) return;

    pane->controllerState = state;
    pane->destroyControllerState = tool_panel_destroy_controller_state;
}

UIPane* tool_panel_bind_dispatch_pane(UIPane* pane) {
    UIPane* previous = s_toolPanelDispatchPane;
    s_toolPanelDispatchPane = pane;
    return previous;
}

void tool_panel_restore_dispatch_pane(UIPane* pane) {
    s_toolPanelDispatchPane = pane;
}

static ToolPanelControllerState* tool_panel_controller_state_for_pane(UIPane* pane) {
    if (!pane) return NULL;

    if (!pane->controllerState || pane->destroyControllerState != tool_panel_destroy_controller_state) {
        tool_panel_attach_controller(pane);
    }
    if (!pane->controllerState || pane->destroyControllerState != tool_panel_destroy_controller_state) {
        return NULL;
    }

    return (ToolPanelControllerState*)pane->controllerState;
}

void* tool_panel_ensure_state_slot_for_pane(UIPane* pane,
                                            ToolPanelStateSlot slot,
                                            size_t size,
                                            void (*init_state)(void*),
                                            void (*destroy_state)(void*)) {
    if (slot < 0 || slot >= TOOL_PANEL_STATE_SLOT_COUNT || size == 0) {
        return NULL;
    }

    ToolPanelControllerState* controller = tool_panel_controller_state_for_pane(pane);
    if (!controller) return NULL;

    if (!controller->slots[slot]) {
        void* state = malloc(size);
        if (!state) return NULL;
        memset(state, 0, size);
        if (init_state) {
            init_state(state);
        }
        controller->slots[slot] = state;
        controller->destroy_slots[slot] = destroy_state;
    }

    return controller->slots[slot];
}

void* tool_panel_ensure_state_slot(ToolPanelStateSlot slot,
                                   size_t size,
                                   void (*init_state)(void*),
                                   void (*destroy_state)(void*)) {
    UIPane* pane = s_toolPanelDispatchPane;
    if (!pane) {
        UIState* ui = getUIState();
        pane = ui ? ui->toolPanel : NULL;
    }
    return tool_panel_ensure_state_slot_for_pane(pane, slot, size, init_state, destroy_state);
}

void* tool_panel_resolve_state_slot(ToolPanelStateSlot slot,
                                    size_t size,
                                    void (*init_state)(void*),
                                    void (*destroy_state)(void*),
                                    void* bootstrap_state,
                                    bool* bootstrap_initialized) {
    void* state = tool_panel_ensure_state_slot(slot, size, init_state, destroy_state);
    if (state) return state;
    if (!bootstrap_state) return NULL;
    if (bootstrap_initialized && !*bootstrap_initialized) {
        if (init_state) {
            init_state(bootstrap_state);
        }
        *bootstrap_initialized = true;
    }
    return bootstrap_state;
}

static void render_project_panel_adapter(UIPane* pane, bool hovered, struct IDECoreState* core) {
    (void)hovered;
    (void)core;
    renderProjectFilesPanel(pane);
}

static void render_libraries_panel_adapter(UIPane* pane, bool hovered, struct IDECoreState* core) {
    (void)hovered;
    (void)core;
    renderLibrariesPanel(pane);
}

static void render_build_output_panel_adapter(UIPane* pane, bool hovered, struct IDECoreState* core) {
    (void)hovered;
    (void)core;
    renderBuildOutputPanel(pane);
}

static void render_errors_panel_adapter(UIPane* pane, bool hovered, struct IDECoreState* core) {
    (void)hovered;
    (void)core;
    renderErrorsPanel(pane);
}

static void render_assets_panel_adapter(UIPane* pane, bool hovered, struct IDECoreState* core) {
    (void)hovered;
    (void)core;
    renderAssetManagerPanel(pane);
}

static void render_tasks_panel_adapter(UIPane* pane, bool hovered, struct IDECoreState* core) {
    (void)hovered;
    (void)core;
    renderTasksPanel(pane);
}

static void render_git_panel_adapter(UIPane* pane, bool hovered, struct IDECoreState* core) {
    (void)hovered;
    (void)core;
    git_panel_prepare_for_render();
    renderGitPanel(pane);
}

static void handle_git_panel_command_adapter(UIPane* pane, InputCommandMetadata meta) {
    handleGitCommand(pane, (InputCommandMetadata){ .cmd = COMMAND_REFRESH_GIT_STATUS });
    handleGitCommand(pane, meta);
}

static const UIPanelViewAdapter s_projectPanelAdapter = {
    .debug_name = "project",
    .render = render_project_panel_adapter,
    .onCommand = handleProjectFilesCommand,
    .onKeyboard = handleProjectFilesKeyboardInput,
    .onMouse = handleProjectFilesMouseInput,
    .onScroll = handleProjectFilesScrollInput,
    .onHover = handleProjectFilesHoverInput,
    .onTextInput = handleProjectFilesKeyboardInput,
};

static const UIPanelViewAdapter s_librariesPanelAdapter = {
    .debug_name = "libraries",
    .render = render_libraries_panel_adapter,
    .onCommand = handleLibrariesCommand,
    .onKeyboard = handleLibrariesKeyboardInput,
    .onMouse = handleLibrariesMouseInput,
    .onScroll = handleLibrariesScrollInput,
    .onHover = handleLibrariesHoverInput,
    .onTextInput = handleLibrariesKeyboardInput,
};

static const UIPanelViewAdapter s_buildOutputPanelAdapter = {
    .debug_name = "build_output",
    .render = render_build_output_panel_adapter,
    .onCommand = handleBuildOutputCommand,
    .onKeyboard = handleBuildOutputKeyboardInput,
    .onMouse = handleBuildOutputMouseInput,
    .onScroll = handleBuildOutputScrollInput,
    .onHover = handleBuildOutputHoverInput,
    .onTextInput = handleBuildOutputKeyboardInput,
};

static const UIPanelViewAdapter s_errorsPanelAdapter = {
    .debug_name = "errors",
    .render = render_errors_panel_adapter,
    .onCommand = handleErrorsCommand,
    .onKeyboard = handleErrorsKeyboardInput,
    .onMouse = handleErrorsMouseInput,
    .onScroll = handleErrorsScrollInput,
    .onHover = handleErrorsHoverInput,
    .onTextInput = handleErrorsKeyboardInput,
};

static const UIPanelViewAdapter s_assetsPanelAdapter = {
    .debug_name = "assets",
    .render = render_assets_panel_adapter,
    .onCommand = handleAssetsCommand,
    .onKeyboard = handleAssetsKeyboardInput,
    .onMouse = handleAssetsMouseInput,
    .onScroll = handleAssetsScrollInput,
    .onHover = handleAssetsHoverInput,
    .onTextInput = handleAssetsKeyboardInput,
};

static const UIPanelViewAdapter s_tasksPanelAdapter = {
    .debug_name = "tasks",
    .render = render_tasks_panel_adapter,
    .onCommand = handleTasksCommand,
    .onKeyboard = handleTasksKeyboardInput,
    .onMouse = handleTasksMouseInput,
    .onScroll = handleTasksScrollInput,
    .onHover = handleTasksHoverInput,
    .onTextInput = handleTasksKeyboardInput,
};

static const UIPanelViewAdapter s_gitPanelAdapter = {
    .debug_name = "git",
    .render = render_git_panel_adapter,
    .onCommand = handle_git_panel_command_adapter,
    .onKeyboard = handleGitKeyboardInput,
    .onMouse = handleGitMouseInput,
    .onScroll = handleGitScrollInput,
    .onHover = handleGitHoverInput,
    .onTextInput = handleGitKeyboardInput,
};

const UIPanelViewAdapter* tool_panel_adapter_for_icon(IconTool icon) {
    switch (icon) {
        case ICON_PROJECT_FILES:
            return &s_projectPanelAdapter;
        case ICON_LIBRARIES:
            return &s_librariesPanelAdapter;
        case ICON_BUILD_OUTPUT:
            return &s_buildOutputPanelAdapter;
        case ICON_ERRORS:
            return &s_errorsPanelAdapter;
        case ICON_ASSET_MANAGER:
            return &s_assetsPanelAdapter;
        case ICON_TASKS:
            return &s_tasksPanelAdapter;
        case ICON_VERSION_CONTROL:
            return &s_gitPanelAdapter;
        default:
            return NULL;
    }
}

const UIPanelViewAdapter* tool_panel_active_adapter(void) {
    return tool_panel_adapter_for_icon(getActiveIcon());
}
