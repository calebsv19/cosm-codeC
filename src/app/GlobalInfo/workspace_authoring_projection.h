#ifndef IDE_WORKSPACE_AUTHORING_PROJECTION_H
#define IDE_WORKSPACE_AUTHORING_PROJECTION_H

#include <stdbool.h>
#include <stdint.h>

#include "ide/Panes/IconBar/icon_bar.h"
#include "ide/UI/ui_state.h"

/* IDEWAP4's portable presentation boundary; runtime/editor state is excluded. */
#define IDE_WORKSPACE_AUTHORING_PROJECTION_SCHEMA_VERSION 1u

typedef enum IDEWorkspaceAuthoringBuiltinModule {
    IDE_WORKSPACE_AUTHORING_MODULE_PROJECT = 0,
    IDE_WORKSPACE_AUTHORING_MODULE_LIBRARIES,
    IDE_WORKSPACE_AUTHORING_MODULE_BUILD_OUTPUT,
    IDE_WORKSPACE_AUTHORING_MODULE_ERRORS,
    IDE_WORKSPACE_AUTHORING_MODULE_ASSETS,
    IDE_WORKSPACE_AUTHORING_MODULE_TASKS,
    IDE_WORKSPACE_AUTHORING_MODULE_VERSION_CONTROL,
    IDE_WORKSPACE_AUTHORING_MODULE_COUNT
} IDEWorkspaceAuthoringBuiltinModule;

typedef enum IDEWorkspaceAuthoringBuiltinModuleTypeId {
    IDE_WORKSPACE_AUTHORING_MODULE_TYPE_PROJECT = 0x49445001u,
    IDE_WORKSPACE_AUTHORING_MODULE_TYPE_LIBRARIES = 0x49445002u,
    IDE_WORKSPACE_AUTHORING_MODULE_TYPE_BUILD_OUTPUT = 0x49445003u,
    IDE_WORKSPACE_AUTHORING_MODULE_TYPE_ERRORS = 0x49445004u,
    IDE_WORKSPACE_AUTHORING_MODULE_TYPE_ASSETS = 0x49445005u,
    IDE_WORKSPACE_AUTHORING_MODULE_TYPE_TASKS = 0x49445006u,
    IDE_WORKSPACE_AUTHORING_MODULE_TYPE_VERSION_CONTROL = 0x49445007u
} IDEWorkspaceAuthoringBuiltinModuleTypeId;

typedef struct IDEWorkspaceAuthoringProjection {
    bool tool_panel_visible;
    bool control_panel_visible;
    bool terminal_visible;
    IconTool active_tool;
} IDEWorkspaceAuthoringProjection;

void ide_workspace_authoring_projection_defaults(IDEWorkspaceAuthoringProjection *projection);
bool ide_workspace_authoring_projection_valid(const IDEWorkspaceAuthoringProjection *projection);
bool ide_workspace_authoring_projection_equal(const IDEWorkspaceAuthoringProjection *left,
                                              const IDEWorkspaceAuthoringProjection *right);
bool ide_workspace_authoring_projection_capture(IDEWorkspaceAuthoringProjection *out_projection,
                                                const UIState *ui_state,
                                                IconTool active_tool);
bool ide_workspace_authoring_projection_apply(const IDEWorkspaceAuthoringProjection *projection,
                                              UIState *ui_state);
IconTool ide_workspace_authoring_projection_icon_for_module(IDEWorkspaceAuthoringBuiltinModule module);
const char *ide_workspace_authoring_projection_module_id(IDEWorkspaceAuthoringBuiltinModule module);
uint32_t ide_workspace_authoring_projection_module_type_id(IDEWorkspaceAuthoringBuiltinModule module);

#endif
