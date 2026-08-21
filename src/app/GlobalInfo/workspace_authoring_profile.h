#ifndef IDE_WORKSPACE_AUTHORING_PROFILE_H
#define IDE_WORKSPACE_AUTHORING_PROFILE_H

#include "app/GlobalInfo/workspace_authoring_projection.h"

typedef enum IDEWorkspaceAuthoringProfileResult {
    IDE_WORKSPACE_AUTHORING_PROFILE_OK = 0,
    IDE_WORKSPACE_AUTHORING_PROFILE_ERR_INVALID_ARG,
    IDE_WORKSPACE_AUTHORING_PROFILE_ERR_IO,
    IDE_WORKSPACE_AUTHORING_PROFILE_ERR_CONTAINER,
    IDE_WORKSPACE_AUTHORING_PROFILE_ERR_SCHEMA,
    IDE_WORKSPACE_AUTHORING_PROFILE_ERR_REQUIREMENTS,
    IDE_WORKSPACE_AUTHORING_PROFILE_ERR_PROJECTION
} IDEWorkspaceAuthoringProfileResult;

/* WAPP v1 uses a core_pack container; it never carries IDE pane geometry. */
#define IDE_WORKSPACE_AUTHORING_PROFILE_SCHEMA_MAJOR 1u
#define IDE_WORKSPACE_AUTHORING_PROFILE_SCHEMA_MINOR 0u

IDEWorkspaceAuthoringProfileResult ide_workspace_authoring_profile_export_file(
    const char *path,
    const IDEWorkspaceAuthoringProjection *projection);
IDEWorkspaceAuthoringProfileResult ide_workspace_authoring_profile_import_file(
    const char *path,
    IDEWorkspaceAuthoringProjection *out_projection);
const char *ide_workspace_authoring_profile_result_string(IDEWorkspaceAuthoringProfileResult result);

#endif
