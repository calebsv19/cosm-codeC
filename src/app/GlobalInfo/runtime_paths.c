#include "runtime_paths.h"

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static char g_resource_root[PATH_MAX];
static char g_executable_dir[PATH_MAX];
static char g_source_label[32];
static bool g_initialized = false;

static bool parse_bool_env(const char* value) {
    if (!value || !value[0]) return false;
    return strcmp(value, "1") == 0 ||
           strcasecmp(value, "true") == 0 ||
           strcasecmp(value, "yes") == 0 ||
           strcasecmp(value, "on") == 0;
}

static bool debug_enabled(void) {
    return parse_bool_env(getenv("IDE_RESOURCE_PATH_DEBUG"));
}

static bool is_absolute_path(const char* path) {
    return path && path[0] == '/';
}

static void safe_copy(char* out, size_t out_cap, const char* text) {
    if (!out || out_cap == 0) return;
    out[0] = '\0';
    if (!text) return;
    strncpy(out, text, out_cap - 1);
    out[out_cap - 1] = '\0';
}

static void trim_trailing_slashes(char* path) {
    if (!path || !path[0]) return;
    size_t len = strlen(path);
    while (len > 1 && path[len - 1] == '/') {
        path[len - 1] = '\0';
        --len;
    }
}

static bool normalize_path_copy(const char* in, char* out, size_t out_cap) {
    char resolved[PATH_MAX];
    if (!in || !in[0] || !out || out_cap == 0) return false;
    if (realpath(in, resolved)) {
        safe_copy(out, out_cap, resolved);
    } else {
        safe_copy(out, out_cap, in);
    }
    trim_trailing_slashes(out);
    return out[0] != '\0';
}

static bool is_dir_path(const char* path) {
    struct stat st;
    if (!path || !path[0]) return false;
    if (stat(path, &st) != 0) return false;
    return S_ISDIR(st.st_mode);
}

static bool is_file_path(const char* path) {
    struct stat st;
    if (!path || !path[0]) return false;
    if (stat(path, &st) != 0) return false;
    return S_ISREG(st.st_mode);
}

static bool join_paths(const char* left, const char* right, char* out, size_t out_cap) {
    if (!out || out_cap == 0) return false;
    out[0] = '\0';
    if (!right || !right[0]) return false;
    if (!left || !left[0] || is_absolute_path(right)) {
        safe_copy(out, out_cap, right);
        return out[0] != '\0';
    }
    snprintf(out, out_cap, "%s/%s", left, right[0] == '/' ? right + 1 : right);
    trim_trailing_slashes(out);
    return out[0] != '\0';
}

static bool path_has_marker(const char* root, const char* marker_rel) {
    char path[PATH_MAX];
    if (!join_paths(root, marker_rel, path, sizeof(path))) return false;
    return is_dir_path(path);
}

static bool is_dev_resource_root(const char* root) {
    return path_has_marker(root, "include/fonts") ||
           path_has_marker(root, "shared/assets/fonts") ||
           path_has_marker(root, "third_party/codework_shared/assets/fonts") ||
           path_has_marker(root, "shared/vk_renderer/shaders") ||
           path_has_marker(root, "third_party/codework_shared/vk_renderer/shaders");
}

static bool parent_path(const char* current, char* out_parent, size_t out_cap) {
    const char* slash;
    size_t len;
    if (!current || !current[0] || !out_parent || out_cap == 0) return false;
    slash = strrchr(current, '/');
    if (!slash) return false;
    if (slash == current) {
        safe_copy(out_parent, out_cap, "/");
        return true;
    }
    len = (size_t)(slash - current);
    if (len + 1 > out_cap) return false;
    memcpy(out_parent, current, len);
    out_parent[len] = '\0';
    return true;
}

static bool find_resource_root_from(const char* start_dir, char* out_root, size_t out_cap) {
    char current[PATH_MAX];
    int depth = 0;
    if (!normalize_path_copy(start_dir, current, sizeof(current))) return false;

    while (depth < 12) {
        if (is_dev_resource_root(current)) {
            safe_copy(out_root, out_cap, current);
            return true;
        }
        if (strcmp(current, "/") == 0) break;
        if (!parent_path(current, current, sizeof(current))) break;
        depth++;
    }
    return false;
}

static bool resolve_executable_path_from_argv(const char* argv0, char* out_path, size_t out_cap) {
    char candidate[PATH_MAX];
    const char* path_env;
    if (!argv0 || !argv0[0]) return false;

    if (strchr(argv0, '/')) {
        if (is_absolute_path(argv0)) {
            safe_copy(candidate, sizeof(candidate), argv0);
        } else {
            char cwd[PATH_MAX];
            if (!getcwd(cwd, sizeof(cwd))) return false;
            if (!join_paths(cwd, argv0, candidate, sizeof(candidate))) return false;
        }
        if (normalize_path_copy(candidate, out_path, out_cap)) return true;
        return false;
    }

    path_env = getenv("PATH");
    if (!path_env || !path_env[0]) return false;

    {
        const char* segment = path_env;
        while (segment && segment[0]) {
            const char* colon = strchr(segment, ':');
            size_t seg_len = colon ? (size_t)(colon - segment) : strlen(segment);
            char path_dir[PATH_MAX];
            if (seg_len == 0 || seg_len >= sizeof(path_dir)) {
                segment = colon ? colon + 1 : NULL;
                continue;
            }
            memcpy(path_dir, segment, seg_len);
            path_dir[seg_len] = '\0';

            if (join_paths(path_dir, argv0, candidate, sizeof(candidate)) &&
                access(candidate, X_OK) == 0) {
                return normalize_path_copy(candidate, out_path, out_cap);
            }
            segment = colon ? colon + 1 : NULL;
        }
    }

    return false;
}

static bool resolve_executable_path(const char* argv0, char* out_path, size_t out_cap) {
    if (resolve_executable_path_from_argv(argv0, out_path, out_cap)) {
        return true;
    }

#if defined(__APPLE__)
    {
        char path[PATH_MAX];
        uint32_t size = (uint32_t)sizeof(path);
        if (_NSGetExecutablePath(path, &size) == 0) {
            return normalize_path_copy(path, out_path, out_cap);
        }
    }
#elif defined(__linux__)
    {
        char path[PATH_MAX];
        ssize_t n = readlink("/proc/self/exe", path, sizeof(path) - 1);
        if (n > 0 && n < (ssize_t)sizeof(path)) {
            path[n] = '\0';
            return normalize_path_copy(path, out_path, out_cap);
        }
    }
#endif

    return false;
}

static bool resolve_executable_dir(const char* argv0, char* out_dir, size_t out_cap) {
    char executable_path[PATH_MAX];
    const char* slash;
    size_t dir_len;
    if (!resolve_executable_path(argv0, executable_path, sizeof(executable_path))) {
        return false;
    }
    slash = strrchr(executable_path, '/');
    if (!slash || slash == executable_path) return false;
    dir_len = (size_t)(slash - executable_path);
    if (dir_len + 1 > out_cap) return false;
    memcpy(out_dir, executable_path, dir_len);
    out_dir[dir_len] = '\0';
    return true;
}

static bool try_bundle_resource_root(const char* executable_dir, char* out_root, size_t out_cap) {
    char candidate[PATH_MAX];
    if (!executable_dir || !executable_dir[0]) return false;

    if (join_paths(executable_dir, "../Resources", candidate, sizeof(candidate)) &&
        is_dir_path(candidate) &&
        normalize_path_copy(candidate, out_root, out_cap)) {
        return true;
    }

    if (join_paths(executable_dir, "resources", candidate, sizeof(candidate)) &&
        is_dir_path(candidate) &&
        normalize_path_copy(candidate, out_root, out_cap)) {
        return true;
    }

    return false;
}

bool ide_runtime_paths_init(const char* argv0) {
    char cwd[PATH_MAX];
    const char* env_resource_root = getenv("IDE_RESOURCE_ROOT");
    bool has_cwd = getcwd(cwd, sizeof(cwd)) != NULL;

    g_resource_root[0] = '\0';
    g_executable_dir[0] = '\0';
    safe_copy(g_source_label, sizeof(g_source_label), "unset");

    if (resolve_executable_dir(argv0, g_executable_dir, sizeof(g_executable_dir))) {
        trim_trailing_slashes(g_executable_dir);
    }

    if (env_resource_root && env_resource_root[0]) {
        char normalized_env_root[PATH_MAX];
        if (normalize_path_copy(env_resource_root, normalized_env_root, sizeof(normalized_env_root)) &&
            is_dir_path(normalized_env_root)) {
            safe_copy(g_resource_root, sizeof(g_resource_root), normalized_env_root);
            safe_copy(g_source_label, sizeof(g_source_label), "env");
        } else if (debug_enabled()) {
            fprintf(stderr,
                    "[RuntimePaths] IDE_RESOURCE_ROOT is set but invalid directory: %s\n",
                    env_resource_root);
        }
    }

    if (!g_resource_root[0] &&
        try_bundle_resource_root(g_executable_dir, g_resource_root, sizeof(g_resource_root))) {
        safe_copy(g_source_label, sizeof(g_source_label), "bundle");
    }

    if (!g_resource_root[0] && g_executable_dir[0] &&
        find_resource_root_from(g_executable_dir, g_resource_root, sizeof(g_resource_root))) {
        safe_copy(g_source_label, sizeof(g_source_label), "exe_scan");
    }

    if (!g_resource_root[0] && has_cwd &&
        find_resource_root_from(cwd, g_resource_root, sizeof(g_resource_root))) {
        safe_copy(g_source_label, sizeof(g_source_label), "cwd_scan");
    }

    if (!g_resource_root[0]) {
        if (has_cwd) {
            safe_copy(g_resource_root, sizeof(g_resource_root), cwd);
            safe_copy(g_source_label, sizeof(g_source_label), "cwd");
        } else {
            safe_copy(g_resource_root, sizeof(g_resource_root), ".");
            safe_copy(g_source_label, sizeof(g_source_label), "dot");
        }
    }

    g_initialized = true;

    if (debug_enabled()) {
        fprintf(stderr,
                "[RuntimePaths] source=%s resource_root=%s executable_dir=%s\n",
                g_source_label,
                g_resource_root[0] ? g_resource_root : "(none)",
                g_executable_dir[0] ? g_executable_dir : "(none)");
    }

    return g_resource_root[0] != '\0';
}

const char* ide_runtime_resource_root(void) {
    if (!g_initialized) {
        ide_runtime_paths_init(NULL);
    }
    return g_resource_root;
}

const char* ide_runtime_executable_dir(void) {
    if (!g_initialized) {
        ide_runtime_paths_init(NULL);
    }
    return g_executable_dir;
}

bool ide_runtime_join_resource_path(const char* relative_path,
                                    char* out_path,
                                    size_t out_cap) {
    const char* root;
    if (!relative_path || !relative_path[0] || !out_path || out_cap == 0) {
        return false;
    }
    if (is_absolute_path(relative_path)) {
        safe_copy(out_path, out_cap, relative_path);
        return out_path[0] != '\0';
    }
    root = ide_runtime_resource_root();
    return join_paths(root, relative_path, out_path, out_cap);
}

bool ide_runtime_probe_resource_path(const char* relative_or_absolute_path,
                                     char* out_path,
                                     size_t out_cap) {
    char candidate[PATH_MAX];
    char cwd[PATH_MAX];
    const char* root;
    const char* exe_dir;

    if (!relative_or_absolute_path || !relative_or_absolute_path[0] ||
        !out_path || out_cap == 0) {
        return false;
    }

    if (is_absolute_path(relative_or_absolute_path)) {
        safe_copy(out_path, out_cap, relative_or_absolute_path);
        return is_file_path(out_path) || is_dir_path(out_path);
    }

    root = ide_runtime_resource_root();
    if (join_paths(root, relative_or_absolute_path, candidate, sizeof(candidate)) &&
        (is_file_path(candidate) || is_dir_path(candidate))) {
        safe_copy(out_path, out_cap, candidate);
        return true;
    }

    exe_dir = ide_runtime_executable_dir();
    if (exe_dir && exe_dir[0] &&
        join_paths(exe_dir, relative_or_absolute_path, candidate, sizeof(candidate)) &&
        (is_file_path(candidate) || is_dir_path(candidate))) {
        safe_copy(out_path, out_cap, candidate);
        return true;
    }

    if (getcwd(cwd, sizeof(cwd)) &&
        join_paths(cwd, relative_or_absolute_path, candidate, sizeof(candidate)) &&
        (is_file_path(candidate) || is_dir_path(candidate))) {
        safe_copy(out_path, out_cap, candidate);
        return true;
    }

    return false;
}
