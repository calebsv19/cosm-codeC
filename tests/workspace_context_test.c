#include "app/GlobalInfo/workspace_context.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

DirEntry* projectRoot = NULL;
char projectPath[1024] = "/tmp/workspace_context/project";
char projectRootPath[1024] = "/tmp/workspace_context/ide";

static char g_workspace_path[1024] = "/tmp/workspace_context/project";
static WorkspaceBuildConfig g_build_config;

const char* getWorkspacePath(void) {
    return g_workspace_path;
}

const WorkspaceBuildConfig* getWorkspaceBuildConfig(void) {
    return &g_build_config;
}

int main(void) {
    snprintf(g_build_config.build_args, sizeof(g_build_config.build_args), "-Iinclude");

    IDEWorkspaceContext context;
    ide_workspace_context_capture(&context);

    assert(context.project_path == projectPath);
    assert(context.workspace_path == g_workspace_path);
    assert(context.ide_root_path == projectRootPath);
    assert(context.build_config == &g_build_config);
    assert(context.build_args == g_build_config.build_args);
    assert(context.has_project_path);
    assert(context.has_workspace_path);
    assert(context.has_build_args);
    assert(ide_workspace_context_path_in_project(&context,
                                                 "/tmp/workspace_context/project/src/main.c"));
    assert(ide_workspace_context_path_in_project(&context,
                                                 "/tmp/workspace_context/project"));
    assert(!ide_workspace_context_path_in_project(&context,
                                                  "/tmp/workspace_context/projectile/main.c"));
    assert(!ide_workspace_context_path_in_project(&context,
                                                  "/tmp/workspace_context/other/main.c"));

    g_build_config.build_args[0] = '\0';
    ide_workspace_context_capture(&context);
    assert(context.build_args == NULL);
    assert(!context.has_build_args);

    printf("workspace_context_test: ok\n");
    return 0;
}
