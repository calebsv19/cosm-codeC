#include "core/Ipc/ide_ipc_path_guard.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set_err(char* out, size_t cap, const char* msg) {
    if (!out || cap == 0) return;
    snprintf(out, cap, "%s", msg ? msg : "Path validation failed");
}

static bool is_within_root(const char* root, const char* path) {
    if (!root || !path) return false;
    size_t rl = strlen(root);
    if (strncmp(root, path, rl) != 0) return false;
    return (path[rl] == '\0' || path[rl] == '/');
}

bool ide_ipc_resolve_workspace_existing_path(const char* project_root,
                                             const char* request_path,
                                             char* out_abs,
                                             size_t out_abs_cap,
                                             char* error_out,
                                             size_t error_out_cap) {
    if (!project_root || !project_root[0] || !request_path || !request_path[0] ||
        !out_abs || out_abs_cap == 0) {
        set_err(error_out, error_out_cap, "Missing path guard inputs");
        return false;
    }

    if (request_path[0] == '/') {
        set_err(error_out, error_out_cap, "Absolute paths are not allowed");
        return false;
    }

    char root_real[PATH_MAX];
    if (!realpath(project_root, root_real)) {
        set_err(error_out, error_out_cap, "Failed to resolve workspace root");
        return false;
    }

    char joined[PATH_MAX];
    int wn = snprintf(joined, sizeof(joined), "%s/%s", root_real, request_path);
    if (wn <= 0 || (size_t)wn >= sizeof(joined)) {
        set_err(error_out, error_out_cap, "Target path exceeds limit");
        return false;
    }

    char target_real[PATH_MAX];
    if (!realpath(joined, target_real)) {
        set_err(error_out, error_out_cap, "Failed to resolve target path");
        return false;
    }

    if (!is_within_root(root_real, target_real)) {
        set_err(error_out, error_out_cap, "Target path escapes workspace root");
        return false;
    }

    snprintf(out_abs, out_abs_cap, "%s", target_real);
    out_abs[out_abs_cap - 1] = '\0';
    return true;
}
