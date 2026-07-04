#include "app/GlobalInfo/workspace_context.h"

#include <string.h>

#include "app/GlobalInfo/core_state.h"

void ide_workspace_context_capture(IDEWorkspaceContext* outContext) {
    if (!outContext) return;
    memset(outContext, 0, sizeof(*outContext));

    outContext->project_root = projectRoot;
    outContext->project_path = projectPath;
    outContext->workspace_path = getWorkspacePath();
    outContext->ide_root_path = projectRootPath;
    outContext->build_config = getWorkspaceBuildConfig();
    outContext->build_args = (outContext->build_config &&
                              outContext->build_config->build_args[0])
        ? outContext->build_config->build_args
        : NULL;
    outContext->has_project_path = outContext->project_path &&
                                    outContext->project_path[0] != '\0';
    outContext->has_workspace_path = outContext->workspace_path &&
                                      outContext->workspace_path[0] != '\0';
    outContext->has_build_args = outContext->build_args &&
                                  outContext->build_args[0] != '\0';
}

bool ide_workspace_context_path_in_project(const IDEWorkspaceContext* context,
                                           const char* file_path) {
    if (!context || !file_path || !file_path[0] || !context->has_project_path) {
        return false;
    }
    size_t root_len = strlen(context->project_path);
    if (root_len == 0) return false;
    if (strncmp(file_path, context->project_path, root_len) != 0) return false;
    char boundary = file_path[root_len];
    return boundary == '\0' || boundary == '/';
}
