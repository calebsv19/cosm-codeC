#include "app/GlobalInfo/workspace_authoring_projection.h"

#include "ide/UI/ui_state.h"

static const char *const k_module_ids[IDE_WORKSPACE_AUTHORING_MODULE_COUNT] = {
    "ide.tool.project", "ide.tool.libraries", "ide.tool.build_output",
    "ide.tool.errors", "ide.tool.assets", "ide.tool.tasks",
    "ide.tool.version_control"};

static const uint32_t k_module_type_ids[IDE_WORKSPACE_AUTHORING_MODULE_COUNT] = {
    IDE_WORKSPACE_AUTHORING_MODULE_TYPE_PROJECT,
    IDE_WORKSPACE_AUTHORING_MODULE_TYPE_LIBRARIES,
    IDE_WORKSPACE_AUTHORING_MODULE_TYPE_BUILD_OUTPUT,
    IDE_WORKSPACE_AUTHORING_MODULE_TYPE_ERRORS,
    IDE_WORKSPACE_AUTHORING_MODULE_TYPE_ASSETS,
    IDE_WORKSPACE_AUTHORING_MODULE_TYPE_TASKS,
    IDE_WORKSPACE_AUTHORING_MODULE_TYPE_VERSION_CONTROL};

void ide_workspace_authoring_projection_defaults(IDEWorkspaceAuthoringProjection *projection) {
    if (!projection) return;
    *projection = (IDEWorkspaceAuthoringProjection){true, true, true, ICON_PROJECT_FILES};
}

bool ide_workspace_authoring_projection_valid(const IDEWorkspaceAuthoringProjection *projection) {
    return projection && projection->active_tool >= ICON_PROJECT_FILES && projection->active_tool < ICON_COUNT;
}

bool ide_workspace_authoring_projection_equal(const IDEWorkspaceAuthoringProjection *left,
                                              const IDEWorkspaceAuthoringProjection *right) {
    return left && right && left->tool_panel_visible == right->tool_panel_visible &&
           left->control_panel_visible == right->control_panel_visible &&
           left->terminal_visible == right->terminal_visible && left->active_tool == right->active_tool;
}

bool ide_workspace_authoring_projection_capture(IDEWorkspaceAuthoringProjection *out_projection,
                                                const UIState *ui_state, IconTool active_tool) {
    if (!out_projection || !ui_state || active_tool < ICON_PROJECT_FILES || active_tool >= ICON_COUNT) return false;
    *out_projection = (IDEWorkspaceAuthoringProjection){ui_state->toolPanelVisible,
                                                         ui_state->controlPanelVisible,
                                                         ui_state->terminalVisible,
                                                         active_tool};
    return true;
}

bool ide_workspace_authoring_projection_apply(const IDEWorkspaceAuthoringProjection *projection,
                                              UIState *ui_state) {
    if (!ide_workspace_authoring_projection_valid(projection) || !ui_state) return false;
    ui_state->toolPanelVisible = projection->tool_panel_visible;
    ui_state->controlPanelVisible = projection->control_panel_visible;
    ui_state->terminalVisible = projection->terminal_visible;
    setActiveIcon(projection->active_tool);
    return true;
}

IconTool ide_workspace_authoring_projection_icon_for_module(IDEWorkspaceAuthoringBuiltinModule module) {
    return module >= IDE_WORKSPACE_AUTHORING_MODULE_PROJECT && module < IDE_WORKSPACE_AUTHORING_MODULE_COUNT
               ? (IconTool)module
               : ICON_COUNT;
}

const char *ide_workspace_authoring_projection_module_id(IDEWorkspaceAuthoringBuiltinModule module) {
    return module >= IDE_WORKSPACE_AUTHORING_MODULE_PROJECT && module < IDE_WORKSPACE_AUTHORING_MODULE_COUNT
               ? k_module_ids[module]
               : NULL;
}

uint32_t ide_workspace_authoring_projection_module_type_id(IDEWorkspaceAuthoringBuiltinModule module) {
    return module >= IDE_WORKSPACE_AUTHORING_MODULE_PROJECT && module < IDE_WORKSPACE_AUTHORING_MODULE_COUNT
               ? k_module_type_ids[module]
               : 0u;
}
