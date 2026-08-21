#ifndef IDE_WORKSPACE_AUTHORING_PROFILE_PATH_H
#define IDE_WORKSPACE_AUTHORING_PROFILE_PATH_H

#include <stdbool.h>
#include <stddef.h>

/* The user-facing S4 control intentionally uses one visible workspace-local slot. */
bool ide_workspace_authoring_profile_default_path(const char *workspace_root,
                                                  char *out_path,
                                                  size_t out_path_capacity);

#endif
