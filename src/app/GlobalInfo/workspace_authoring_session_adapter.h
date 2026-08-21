#ifndef IDE_WORKSPACE_AUTHORING_SESSION_ADAPTER_H
#define IDE_WORKSPACE_AUTHORING_SESSION_ADAPTER_H

#include "app/GlobalInfo/workspace_authoring_host.h"
#include "app/GlobalInfo/workspace_authoring_profile.h"

void ide_workspace_authoring_session_adapter_reset(IDEWorkspaceAuthoringHost *host);
CoreWorkspaceAuthoringSessionOutcome ide_workspace_authoring_session_adapter_enter(
    IDEWorkspaceAuthoringHost *host);
CoreWorkspaceAuthoringSessionOutcome ide_workspace_authoring_session_adapter_apply(
    IDEWorkspaceAuthoringHost *host);
CoreWorkspaceAuthoringSessionOutcome ide_workspace_authoring_session_adapter_cancel(
    IDEWorkspaceAuthoringHost *host);
bool ide_workspace_authoring_session_adapter_preview_presentation(
    IDEWorkspaceAuthoringHost *host,
    const IDEWorkspaceAuthoringProjection *draft);
IDEWorkspaceAuthoringProfileResult ide_workspace_authoring_session_adapter_preview_profile_file(
    IDEWorkspaceAuthoringHost *host,
    const char *path);

#endif
