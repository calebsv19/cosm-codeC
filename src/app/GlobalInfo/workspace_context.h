#ifndef WORKSPACE_CONTEXT_H
#define WORKSPACE_CONTEXT_H

#include <stdbool.h>

#include "app/GlobalInfo/project.h"
#include "app/GlobalInfo/workspace_prefs.h"

typedef struct {
    const DirEntry* project_root;
    const char* project_path;
    const char* workspace_path;
    const char* ide_root_path;
    const WorkspaceBuildConfig* build_config;
    const char* build_args;
    bool has_project_path;
    bool has_workspace_path;
    bool has_build_args;
} IDEWorkspaceContext;

void ide_workspace_context_capture(IDEWorkspaceContext* outContext);
bool ide_workspace_context_path_in_project(const IDEWorkspaceContext* context,
                                           const char* file_path);

#endif // WORKSPACE_CONTEXT_H
