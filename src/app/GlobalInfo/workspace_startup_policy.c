#include "workspace_startup_policy.h"

#include <stdio.h>
#include <string.h>

bool ide_workspace_startup_build_default_root(const char* override_path,
                                              const char* home_dir,
                                              const char* cwd_path,
                                              char* out_path,
                                              size_t out_cap) {
    int written = -1;
    if (!out_path || out_cap == 0) {
        return false;
    }

    out_path[0] = '\0';
    if (override_path && override_path[0]) {
        written = snprintf(out_path, out_cap, "%s", override_path);
        return written >= 0 && (size_t)written < out_cap;
    }
    if (home_dir && home_dir[0]) {
        written = snprintf(out_path, out_cap, "%s/Desktop/CodeWork", home_dir);
        return written >= 0 && (size_t)written < out_cap;
    }
    if (cwd_path && cwd_path[0]) {
        written = snprintf(out_path, out_cap, "%s", cwd_path);
        return written >= 0 && (size_t)written < out_cap;
    }
    written = snprintf(out_path, out_cap, ".");
    return written >= 0 && (size_t)written < out_cap;
}

bool ide_workspace_startup_select_root(const char* stored_path,
                                       bool stored_path_valid,
                                       const char* default_path,
                                       bool default_path_valid,
                                       char* out_path,
                                       size_t out_cap,
                                       bool* out_used_fallback) {
    int written = -1;
    if (!out_path || out_cap == 0) {
        return false;
    }

    out_path[0] = '\0';
    if (out_used_fallback) {
        *out_used_fallback = false;
    }

    if (stored_path && stored_path[0] && stored_path_valid) {
        written = snprintf(out_path, out_cap, "%s", stored_path);
        return written >= 0 && (size_t)written < out_cap;
    }
    if (default_path && default_path[0] && default_path_valid) {
        if (out_used_fallback && stored_path && stored_path[0]) {
            *out_used_fallback = true;
        }
        written = snprintf(out_path, out_cap, "%s", default_path);
        return written >= 0 && (size_t)written < out_cap;
    }
    return false;
}
