#include "app/GlobalInfo/workspace_authoring_profile_path.h"

#include <stdio.h>

bool ide_workspace_authoring_profile_default_path(const char *workspace_root,
                                                  char *out_path,
                                                  size_t out_path_capacity) {
    if (!workspace_root || !workspace_root[0] || !out_path || out_path_capacity == 0u) return false;
    return snprintf(out_path, out_path_capacity, "%s/ide_files/workspace_authoring.wapp", workspace_root) <
           (int)out_path_capacity;
}
