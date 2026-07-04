#include "core/Analysis/include_path_resolver.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdbool.h>
#include <json-c/json.h>

#include "core/Analysis/analysis_artifact_io.h"

static char* trim_in_place(char* text);

static char* dup_str(const char* p) {
    if (!p) return NULL;
    size_t len = strlen(p);
    char* out = malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, p, len);
    out[len] = '\0';
    return out;
}

static int add_unique(char*** list, size_t* count, size_t* cap, const char* value) {
    if (!list || !count || !cap || !value || !*value) return 0;
    for (size_t i = 0; i < *count; ++i) {
        if ((*list)[i] && strcmp((*list)[i], value) == 0) return 1;
    }
    if (*count >= *cap) {
        size_t newCap = (*cap == 0) ? 8 : (*cap * 2);
        char** tmp = realloc(*list, newCap * sizeof(char*));
        if (!tmp) return 0;
        *list = tmp;
        *cap = newCap;
    }
    (*list)[(*count)++] = dup_str(value);
    return 1;
}

static bool has_unexpanded_make_var(const char* s) {
    if (!s || !*s) return false;
    return strstr(s, "$(") != NULL || strstr(s, "${") != NULL;
}

static bool env_truthy(const char* name) {
    const char* value = getenv(name);
    if (!value || !*value) return false;
    return strcmp(value, "1") == 0 ||
           strcmp(value, "true") == 0 ||
           strcmp(value, "TRUE") == 0 ||
           strcmp(value, "yes") == 0 ||
           strcmp(value, "YES") == 0 ||
           strcmp(value, "on") == 0 ||
           strcmp(value, "ON") == 0;
}

static bool run_command_capture(const char* cwd,
                                char* const argv[],
                                char* output,
                                size_t outputSize) {
    if (!argv || !argv[0] || !output || outputSize == 0) return false;
    output[0] = '\0';

    int pipefd[2];
    if (pipe(pipefd) != 0) return false;

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return false;
    }

    if (pid == 0) {
        close(pipefd[0]);
        if (cwd && *cwd) {
            if (chdir(cwd) != 0) {
                _exit(127);
            }
        }
        if (dup2(pipefd[1], STDOUT_FILENO) < 0) {
            _exit(127);
        }
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        close(pipefd[1]);
        execvp(argv[0], argv);
        _exit(127);
    }

    close(pipefd[1]);

    size_t used = 0;
    while (used + 1 < outputSize) {
        ssize_t n = read(pipefd[0], output + used, outputSize - used - 1);
        if (n > 0) {
            used += (size_t)n;
            continue;
        }
        if (n == 0) break;
        if (errno == EINTR) continue;
        break;
    }
    output[used] = '\0';
    close(pipefd[0]);

    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno == EINTR) continue;
        return false;
    }

    return used > 0 && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static bool run_command_first_line(const char* cwd,
                                   char* const argv[],
                                   char* line,
                                   size_t lineSize) {
    if (!line || lineSize == 0) return false;
    line[0] = '\0';

    char output[4096];
    if (!run_command_capture(cwd, argv, output, sizeof(output))) return false;

    char* first = output;
    char* newline = strpbrk(first, "\r\n");
    if (newline) *newline = '\0';
    first = trim_in_place(first);
    if (!first || !*first) return false;

    snprintf(line, lineSize, "%s", first);
    return true;
}

static char* trim_in_place(char* text) {
    if (!text) return NULL;
    while (*text == ' ' || *text == '\t' || *text == '\n' || *text == '\r') {
        text++;
    }
    size_t len = strlen(text);
    while (len > 0) {
        char c = text[len - 1];
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') break;
        text[--len] = '\0';
    }
    return text;
}

static void parse_overlay_mode(const char* mode, uint64_t* overlays) {
    if (!mode || !overlays || has_unexpanded_make_var(mode)) return;
    if (strcmp(mode, "0") == 0 || strcmp(mode, "off") == 0 || strcmp(mode, "none") == 0) {
        *overlays = 0;
        return;
    }
    if (strcmp(mode, "1") == 0 || strcmp(mode, "on") == 0 || strcmp(mode, "all") == 0) {
        *overlays |= BUILD_FLAG_OVERLAY_ALL;
        return;
    }

    char buf[512];
    size_t len = strlen(mode);
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    memcpy(buf, mode, len);
    buf[len] = '\0';

    char* save = NULL;
    for (char* tok = strtok_r(buf, ",", &save); tok; tok = strtok_r(NULL, ",", &save)) {
        char* part = trim_in_place(tok);
        if (!part || !*part) continue;
        if (strcmp(part, "all") == 0) {
            *overlays |= BUILD_FLAG_OVERLAY_ALL;
        } else if (strcmp(part, "ide") == 0 ||
                   strcmp(part, "ide-metadata") == 0 ||
                   strcmp(part, "ide_metadata") == 0) {
            *overlays |= BUILD_FLAG_OVERLAY_IDE_METADATA;
        } else if (strcmp(part, "units") == 0 ||
                   strcmp(part, "physics-units") == 0 ||
                   strcmp(part, "physics_units") == 0) {
            *overlays |= BUILD_FLAG_OVERLAY_PHYSICS_UNITS;
        } else if (strcmp(part, "memory-check") == 0 ||
                   strcmp(part, "memory_check") == 0 ||
                   strcmp(part, "memcheck") == 0) {
            *overlays |= BUILD_FLAG_OVERLAY_MEMORY_CHECK;
        }
    }
}

static bool contains_bytes(const char* text, size_t length, const char* needle) {
    if (!text || !needle || !*needle) return false;
    size_t needle_len = strlen(needle);
    if (needle_len == 0 || length < needle_len) return false;
    for (size_t i = 0; i + needle_len <= length; ++i) {
        if (memcmp(text + i, needle, needle_len) == 0) return true;
    }
    return false;
}

bool build_flags_source_requests_physics_units(const char* source, size_t length) {
    if (!source || length == 0) return false;
    return contains_bytes(source, length, "[[fisics::dim") ||
           contains_bytes(source, length, "[[fisics::unit") ||
           contains_bytes(source, length, "fisics::dim(") ||
           contains_bytes(source, length, "fisics::unit(");
}

void build_flags_enable_overlays_for_source(BuildFlagSet* set, const char* source, size_t length) {
    if (!set) return;
    if (build_flags_source_requests_physics_units(source, length)) {
        set->overlay_features |= BUILD_FLAG_OVERLAY_PHYSICS_UNITS;
    }
}

static void expand_relative(const char* project_root, const char* raw, char* out, size_t outSize) {
    if (!out || outSize == 0) return;
    out[0] = '\0';
    if (!raw || !*raw) return;
    if (raw[0] == '/' || raw[0] == '\\') {
        strncpy(out, raw, outSize - 1);
        out[outSize - 1] = '\0';
        return;
    }
    if (raw[0] == '~') {
        const char* home = getenv("HOME");
        if (!home) home = "";
        if (raw[1] == '/' || raw[1] == '\\') {
            snprintf(out, outSize, "%s/%s", home, raw + 2);
        } else if (raw[1] == '\0') {
            snprintf(out, outSize, "%s", home);
        } else {
            snprintf(out, outSize, "%s/%s", home, raw + 1);
        }
        return;
    }
    if (project_root && *project_root) {
        snprintf(out, outSize, "%s/%s", project_root, raw);
    } else {
        strncpy(out, raw, outSize - 1);
        out[outSize - 1] = '\0';
    }
}

static bool has_source_extension(const char* path) {
    if (!path) return false;
    const char* ext = strrchr(path, '.');
    return ext && (strcmp(ext, ".c") == 0 || strcmp(ext, ".h") == 0);
}

static bool should_skip_source_scan_dir(const char* name) {
    if (!name) return true;
    return strcmp(name, ".") == 0 ||
           strcmp(name, "..") == 0 ||
           strcmp(name, "build") == 0 ||
           strcmp(name, "ide_files") == 0 ||
           strcmp(name, ".git") == 0 ||
           strcmp(name, ".DS_Store") == 0;
}

static void scan_source_file_for_overlay(const char* path, uint64_t* overlays) {
    if (!path || !overlays || (*overlays & BUILD_FLAG_OVERLAY_PHYSICS_UNITS)) return;

    FILE* f = fopen(path, "rb");
    if (!f) return;
    char buf[8192];
    size_t n = 0;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        if (build_flags_source_requests_physics_units(buf, n)) {
            *overlays |= BUILD_FLAG_OVERLAY_PHYSICS_UNITS;
            break;
        }
    }
    fclose(f);
}

static void scan_source_tree_for_overlays(const char* root, uint64_t* overlays) {
    if (!root || !*root || !overlays || (*overlays & BUILD_FLAG_OVERLAY_PHYSICS_UNITS)) return;

    DIR* dir = opendir(root);
    if (!dir) return;

    struct dirent* ent;
    char child[PATH_MAX];
    while ((ent = readdir(dir)) != NULL) {
        if (*overlays & BUILD_FLAG_OVERLAY_PHYSICS_UNITS) break;
        if (should_skip_source_scan_dir(ent->d_name)) continue;
        snprintf(child, sizeof(child), "%s/%s", root, ent->d_name);
        struct stat st;
        if (stat(child, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            scan_source_tree_for_overlays(child, overlays);
        } else if (S_ISREG(st.st_mode) && has_source_extension(child)) {
            scan_source_file_for_overlay(child, overlays);
        }
    }
    closedir(dir);
}

static void parse_manifest_string_array(json_object* root,
                                        const char* key,
                                        const char* project_root,
                                        char*** includes,
                                        size_t* icount,
                                        size_t* icap,
                                        char*** macros,
                                        size_t* mcount,
                                        size_t* mcap,
                                        uint64_t* overlays) {
    json_object* arr = NULL;
    if (!json_object_object_get_ex(root, key, &arr) || !arr || !json_object_is_type(arr, json_type_array)) {
        return;
    }
    const size_t count = json_object_array_length(arr);
    for (size_t i = 0; i < count; ++i) {
        json_object* item = json_object_array_get_idx(arr, i);
        const char* value = item ? json_object_get_string(item) : NULL;
        if (!value || !*value || has_unexpanded_make_var(value)) continue;
        if (strcmp(key, "include_dirs") == 0) {
            char expanded[PATH_MAX];
            expand_relative(project_root, value, expanded, sizeof(expanded));
            add_unique(includes, icount, icap, expanded);
        } else if (strcmp(key, "defines") == 0) {
            add_unique(macros, mcount, mcap, value);
        } else if (strcmp(key, "overlays") == 0) {
            parse_overlay_mode(value, overlays);
        }
    }
}

static void parse_project_manifest(const char* project_root,
                                   char*** includes,
                                   size_t* icount,
                                   size_t* icap,
                                   char*** macros,
                                   size_t* mcount,
                                   size_t* mcap,
                                   uint64_t* overlays) {
    if (!project_root || !*project_root) return;
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/project.fisics.json", project_root);
    json_object* manifest = json_object_from_file(path);
    if (!manifest || !json_object_is_type(manifest, json_type_object)) {
        if (manifest) json_object_put(manifest);
        return;
    }

    json_object* defaults = NULL;
    if (json_object_object_get_ex(manifest, "defaults", &defaults) &&
        defaults && json_object_is_type(defaults, json_type_object)) {
        parse_manifest_string_array(defaults, "include_dirs", project_root,
                                    includes, icount, icap,
                                    macros, mcount, mcap,
                                    overlays);
        parse_manifest_string_array(defaults, "defines", project_root,
                                    includes, icount, icap,
                                    macros, mcount, mcap,
                                    overlays);
        parse_manifest_string_array(defaults, "overlays", project_root,
                                    includes, icount, icap,
                                    macros, mcount, mcap,
                                    overlays);
    }

    json_object_put(manifest);
}

// Split a colon-separated env var.
static void parse_env_paths(const char* project_root,
                            const char* env,
                            char*** includes,
                            size_t* icount,
                            size_t* icap) {
    if (!env) return;
    const char* p = env;
    while (*p) {
        const char* start = p;
        while (*p && *p != ':') p++;
        size_t len = (size_t)(p - start);
        if (len > 0) {
            char tmp[PATH_MAX];
            if (len >= sizeof(tmp)) len = sizeof(tmp) - 1;
            memcpy(tmp, start, len);
            tmp[len] = '\0';
            char expanded[PATH_MAX];
            expand_relative(project_root, tmp, expanded, sizeof(expanded));
            add_unique(includes, icount, icap, expanded);
        }
        if (*p == ':') p++;
    }
}

static void parse_flags_for_includes_and_macros(const char* project_root,
                                                const char* flags,
                                                char*** includes,
                                                size_t* icount,
                                                size_t* icap,
                                                char*** macros,
                                                size_t* mcount,
                                                size_t* mcap,
                                                uint64_t* overlays,
                                                char* lastSysroot,
                                                size_t lastSysrootSize) {
    if (!flags) return;
    const char* delim = " \t\r\n";
    char buf[32768];
    size_t flen = strlen(flags);
    if (flen >= sizeof(buf)) flen = sizeof(buf) - 1;
    memcpy(buf, flags, flen);
    buf[flen] = '\0';

    char* save = NULL;
    for (char* tok = strtok_r(buf, delim, &save); tok; tok = strtok_r(NULL, delim, &save)) {
        if (strcmp(tok, "-I") == 0 || strcmp(tok, "-isystem") == 0) {
            char* next = strtok_r(NULL, delim, &save);
            if (!next) continue;
            if (has_unexpanded_make_var(next)) continue;
            char expanded[PATH_MAX];
            expand_relative(project_root, next, expanded, sizeof(expanded));
            add_unique(includes, icount, icap, expanded);
        } else if (strncmp(tok, "-I", 2) == 0) {
            const char* raw = tok + 2;
            if (*raw) {
                if (has_unexpanded_make_var(raw)) continue;
                char expanded[PATH_MAX];
                expand_relative(project_root, raw, expanded, sizeof(expanded));
                add_unique(includes, icount, icap, expanded);
            }
        } else if (strncmp(tok, "-isystem", 8) == 0) {
            const char* raw = tok + 8;
            if (*raw) {
                if (has_unexpanded_make_var(raw)) continue;
                char expanded[PATH_MAX];
                expand_relative(project_root, raw, expanded, sizeof(expanded));
                add_unique(includes, icount, icap, expanded);
            }
        } else if (strcmp(tok, "-D") == 0) {
            char* def = strtok_r(NULL, delim, &save);
            if (def && *def && !has_unexpanded_make_var(def)) {
                add_unique(macros, mcount, mcap, def);
            }
        } else if (strncmp(tok, "-D", 2) == 0) {
            const char* def = tok + 2;
            if (*def && !has_unexpanded_make_var(def)) {
                add_unique(macros, mcount, mcap, def);
            }
        } else if (strcmp(tok, "--overlay") == 0) {
            char* mode = strtok_r(NULL, delim, &save);
            parse_overlay_mode(mode, overlays);
        } else if (strncmp(tok, "--overlay=", 10) == 0) {
            parse_overlay_mode(tok + 10, overlays);
        } else if (strncmp(tok, "-isysroot", 9) == 0) {
            const char* root = tok + 9;
            if (!*root) {
                char* next = strtok_r(NULL, delim, &save);
                root = next;
            }
            if (root && *root) {
                if (has_unexpanded_make_var(root)) continue;
                if (lastSysroot && lastSysrootSize > 0) {
                    strncpy(lastSysroot, root, lastSysrootSize - 1);
                    lastSysroot[lastSysrootSize - 1] = '\0';
                }
                char expanded[PATH_MAX];
                expand_relative(project_root, root, expanded, sizeof(expanded));
                char inc[PATH_MAX];
                snprintf(inc, sizeof(inc), "%s/usr/include", expanded);
                add_unique(includes, icount, icap, inc);
            }
        }
    }
}

static void add_default_includes(const char* project_root,
                                 char*** includes,
                                 size_t* icount,
                                 size_t* icap) {
    const char* defaults[] = {
        project_root,
        "src",
        "include",
        "/opt/homebrew/opt/llvm/include",
        "/usr/local/opt/llvm/include",
        "/opt/homebrew/include",
        "/opt/homebrew/include/SDL2",
        "/usr/local/include",
        "/usr/include",
        "/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include",
        "../fisics/src"
    };
    for (size_t i = 0; i < sizeof(defaults)/sizeof(defaults[0]); ++i) {
        char expanded[PATH_MAX];
        expand_relative(project_root, defaults[i], expanded, sizeof(expanded));
        add_unique(includes, icount, icap, expanded);
    }

    const char* sdkroot = getenv("SDKROOT");
    if (sdkroot && *sdkroot) {
        char inc[PATH_MAX];
        snprintf(inc, sizeof(inc), "%s/usr/include", sdkroot);
        add_unique(includes, icount, icap, inc);
        snprintf(inc, sizeof(inc), "%s/System/Library/Frameworks", sdkroot);
        add_unique(includes, icount, icap, inc);
    }

    const char* sysenv = getenv("SYSTEM_INCLUDE_PATHS");
    if (sysenv && *sysenv) {
        parse_env_paths(project_root, sysenv, includes, icount, icap);
    }

    // Framework-style includes
    struct stat st;
    if (stat("/Library/Frameworks/SDL2.framework/Headers", &st) == 0 && S_ISDIR(st.st_mode)) {
        add_unique(includes, icount, icap, "/Library/Frameworks/SDL2.framework/Headers");
    }
}

static void parse_makefiles(const char* project_root,
                            char*** includes,
                            size_t* icount,
                            size_t* icap,
                            char*** macros,
                            size_t* mcount,
                            size_t* mcap,
                            uint64_t* overlays,
                            char* lastSysroot,
                            size_t lastSysrootSize) {
    char mf[PATH_MAX];
    snprintf(mf, sizeof(mf), "%s/Makefile", project_root ? project_root : "");
    FILE* f = fopen(mf, "r");
    if (!f) {
        snprintf(mf, sizeof(mf), "%s/makefile", project_root ? project_root : "");
        f = fopen(mf, "r");
    }
    if (!f) return;
    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        parse_flags_for_includes_and_macros(project_root, line,
                                            includes, icount, icap,
                                            macros, mcount, mcap,
                                            overlays,
                                            lastSysroot, lastSysrootSize);
    }
    fclose(f);
}

// Running make can evaluate Makefile $(shell ...) expressions. Keep this behind
// an explicit trust opt-in so passive workspace analysis does not execute
// workspace-controlled commands by default.
static void parse_make_vars(const char* project_root,
                            char*** includes,
                            size_t* icount,
                            size_t* icap,
                            char*** macros,
                            size_t* mcount,
                            size_t* mcap,
                            uint64_t* overlays,
                            char* lastSysroot,
                            size_t lastSysrootSize) {
    if (!project_root || !*project_root) return;
    if (!env_truthy("IDE_TRUST_WORKSPACE_MAKE_VARS")) return;

    char* const argv[] = { "make", "-pn", NULL };
    char output[1024 * 1024];
    if (!run_command_capture(project_root, argv, output, sizeof(output))) return;

    const char* interesting[] = {
        "CPPFLAGS",
        "CFLAGS",
        "COMMON_CFLAGS",
        "CLANG_CFLAGS",
        "INCLUDES",
        "INC_DIRS",
        "SDL_CFLAGS",
        "SDL2_CFLAGS",
        "SDL_TTF_CFLAGS",
        "FISICS_FLAGS",
        "SDLAPP_DIR",
        "SDKROOT"
    };
    char* saveLine = NULL;
    for (char* line = strtok_r(output, "\n", &saveLine);
         line;
         line = strtok_r(NULL, "\n", &saveLine)) {
        for (size_t i = 0; i < sizeof(interesting)/sizeof(interesting[0]); ++i) {
            const char* key = interesting[i];
            size_t klen = strlen(key);
            if (strncmp(line, key, klen) == 0) {
                const char* eq = strchr(line, '=');
                if (!eq) continue;
                const char* val = eq + 1;
                while (*val == ' ' || *val == '\t') val++;
                if (strcmp(key, "SDLAPP_DIR") == 0) {
                    char expanded[PATH_MAX];
                    expand_relative(project_root, val, expanded, sizeof(expanded));
                    // trim trailing whitespace/newline
                    size_t len = strlen(expanded);
                    while (len > 0 && (expanded[len - 1] == '\n' || expanded[len - 1] == '\r' || expanded[len - 1] == ' ' || expanded[len - 1] == '\t')) {
                        expanded[len - 1] = '\0';
                        len--;
                    }
                    add_unique(includes, icount, icap, expanded);
                } else {
                    parse_flags_for_includes_and_macros(project_root, val,
                                                        includes, icount, icap,
                                                        macros, mcount, mcap,
                                                        overlays,
                                                        lastSysroot, lastSysrootSize);
                }
            }
        }
    }
}

static void add_clang_resource_include(char*** includes,
                                       size_t* icount,
                                       size_t* icap) {
    const char* candidates[] = { "clang", "clang++" };
    char line[PATH_MAX];
    for (size_t i = 0; i < sizeof(candidates)/sizeof(candidates[0]); ++i) {
        char* const argv[] = { (char*)candidates[i], "-print-resource-dir", NULL };
        if (!run_command_first_line(NULL, argv, line, sizeof(line))) continue;
        char inc[PATH_MAX];
        snprintf(inc, sizeof(inc), "%s/include", line);
        add_unique(includes, icount, icap, inc);
        return;
    }
}

static bool run_llvm_config_cflags(const char* command,
                                   const char* project_root,
                                   char* line,
                                   size_t lineSize) {
    if (!command || !command[0] || !line || lineSize == 0) return false;

    char* const argv[] = { (char*)command, "--cflags", NULL };
    return run_command_first_line(project_root, argv, line, lineSize);
}

static void add_llvm_config_flags(const char* project_root,
                                  char*** includes,
                                  size_t* icount,
                                  size_t* icap,
                                  char*** macros,
                                  size_t* mcount,
                                  size_t* mcap,
                                  uint64_t* overlays,
                                  char* lastSysroot,
                                  size_t lastSysrootSize) {
    char line[2048];
    const char* candidates[] = {
        "/opt/homebrew/opt/llvm/bin/llvm-config",
        "/opt/homebrew/bin/llvm-config",
        "/usr/local/opt/llvm/bin/llvm-config",
        "/usr/local/bin/llvm-config",
        "llvm-config"
    };

    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
        if (!run_llvm_config_cflags(candidates[i], project_root, line, sizeof(line))) {
            continue;
        }
        parse_flags_for_includes_and_macros(project_root, line,
                                            includes, icount, icap,
                                            macros, mcount, mcap,
                                            overlays,
                                            lastSysroot, lastSysrootSize);
        return;
    }
}

size_t gather_build_flags(const char* project_root,
                          const char* extra_flags,
                          BuildFlagSet* out) {
    if (out) {
        out->include_paths = NULL;
        out->include_count = 0;
        out->macro_defines = NULL;
        out->macro_count = 0;
        out->overlay_features = 0;
    }

    char** incs = NULL;
    char** defs = NULL;
    size_t icount = 0, dcount = 0;
    size_t icap = 0, dcap = 0;
    uint64_t overlays = 0;

    parse_project_manifest(project_root,
                           &incs, &icount, &icap,
                           &defs, &dcount, &dcap,
                           &overlays);
    char lastSysroot[PATH_MAX] = {0};
    parse_makefiles(project_root, &incs, &icount, &icap, &defs, &dcount, &dcap,
                    &overlays,
                    lastSysroot, sizeof(lastSysroot));
    parse_make_vars(project_root, &incs, &icount, &icap, &defs, &dcount, &dcap,
                    &overlays,
                    lastSysroot, sizeof(lastSysroot));
    add_llvm_config_flags(project_root,
                          &incs, &icount, &icap,
                          &defs, &dcount, &dcap,
                          &overlays,
                          lastSysroot, sizeof(lastSysroot));
    add_default_includes(project_root, &incs, &icount, &icap);
    parse_flags_for_includes_and_macros(project_root, extra_flags,
                                        &incs, &icount, &icap,
                                        &defs, &dcount, &dcap,
                                        &overlays,
                                        lastSysroot, sizeof(lastSysroot));
    // If sysroot was captured, add frameworks root as well.
    if (lastSysroot[0]) {
        char fw[PATH_MAX];
        snprintf(fw, sizeof(fw), "%s/System/Library/Frameworks", lastSysroot);
        add_unique(&incs, &icount, &icap, fw);
    }
    add_clang_resource_include(&incs, &icount, &icap);
    if (!(overlays & BUILD_FLAG_OVERLAY_PHYSICS_UNITS)) {
        scan_source_tree_for_overlays(project_root, &overlays);
    }

    const char* debugEnv = getenv("ANALYSIS_FLAGS_DEBUG");
    bool debug = (debugEnv && debugEnv[0] && debugEnv[0] != '0');
    if (debug) {
        printf("[FlagsDebug] Includes (%zu):\n", icount);
        for (size_t i = 0; i < icount; ++i) {
            printf("  - %s\n", incs[i] ? incs[i] : "(null)");
        }
        printf("[FlagsDebug] Macros (%zu):\n", dcount);
        for (size_t i = 0; i < dcount; ++i) {
            printf("  - %s\n", defs[i] ? defs[i] : "(null)");
        }
        printf("[FlagsDebug] Overlays: 0x%llx\n", (unsigned long long)overlays);
    }

    if (out) {
        out->include_paths = incs;
        out->include_count = icount;
        out->macro_defines = defs;
        out->macro_count = dcount;
        out->overlay_features = overlays;
    } else {
        for (size_t i = 0; i < icount; ++i) free(incs[i]);
        free(incs);
        for (size_t i = 0; i < dcount; ++i) free(defs[i]);
        free(defs);
    }
    return icount;
}

void free_build_flag_set(BuildFlagSet* set) {
    if (!set) return;
    if (set->include_paths) {
        for (size_t i = 0; i < set->include_count; ++i) free(set->include_paths[i]);
        free(set->include_paths);
    }
    if (set->macro_defines) {
        for (size_t i = 0; i < set->macro_count; ++i) free(set->macro_defines[i]);
        free(set->macro_defines);
    }
    memset(set, 0, sizeof(*set));
}

void save_build_flags(const BuildFlagSet* set, const char* workspace_root) {
    if (!workspace_root || !*workspace_root || !set) return;

    json_object* root = json_object_new_object();
    json_object* incs = json_object_new_array();
    for (size_t i = 0; i < set->include_count; ++i) {
        const char* v = set->include_paths ? set->include_paths[i] : NULL;
        json_object_array_add(incs, json_object_new_string(v ? v : ""));
    }
    json_object* defs = json_object_new_array();
    for (size_t i = 0; i < set->macro_count; ++i) {
        const char* v = set->macro_defines ? set->macro_defines[i] : NULL;
        json_object_array_add(defs, json_object_new_string(v ? v : ""));
    }
    json_object_object_add(root, "include_paths", incs);
    json_object_object_add(root, "macro_defines", defs);
    json_object_object_add(root,
                           "overlay_features",
                           json_object_new_int64((int64_t)set->overlay_features));

    const char* serialized = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
    if (serialized) {
        analysis_artifact_io_write_text(workspace_root, "build_flags.json", serialized);
    }
    json_object_put(root);
}

void load_build_flags(BuildFlagSet* set, const char* workspace_root) {
    if (!set) return;
    free_build_flag_set(set);
    if (!workspace_root || !*workspace_root) return;

    char* buf = analysis_artifact_io_read_text(workspace_root,
                                               "build_flags.json",
                                               ANALYSIS_ARTIFACT_IO_DEFAULT_MAX_BYTES,
                                               NULL);
    if (!buf) return;

    json_object* root = json_tokener_parse(buf);
    free(buf);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        return;
    }

    size_t icap = 0, dcap = 0;
    json_object* jincs = NULL;
    if (json_object_object_get_ex(root, "include_paths", &jincs) &&
        jincs && json_object_is_type(jincs, json_type_array)) {
        size_t count = json_object_array_length(jincs);
        set->include_paths = calloc(count, sizeof(char*));
        set->include_count = 0;
        icap = count;
        for (size_t i = 0; i < count; ++i) {
            json_object* ji = json_object_array_get_idx(jincs, i);
            const char* v = ji ? json_object_get_string(ji) : NULL;
            if (has_unexpanded_make_var(v)) continue;
            add_unique(&set->include_paths, &set->include_count, &icap, v ? v : "");
        }
    }

    json_object* jdefs = NULL;
    if (json_object_object_get_ex(root, "macro_defines", &jdefs) &&
        jdefs && json_object_is_type(jdefs, json_type_array)) {
        size_t count = json_object_array_length(jdefs);
        set->macro_defines = calloc(count, sizeof(char*));
        set->macro_count = 0;
        dcap = count;
        for (size_t i = 0; i < count; ++i) {
            json_object* jd = json_object_array_get_idx(jdefs, i);
            const char* v = jd ? json_object_get_string(jd) : NULL;
            if (has_unexpanded_make_var(v)) continue;
            add_unique(&set->macro_defines, &set->macro_count, &dcap, v ? v : "");
        }
    }

    json_object* jov = NULL;
    if (json_object_object_get_ex(root, "overlay_features", &jov) && jov) {
        set->overlay_features = (uint64_t)json_object_get_int64(jov);
    }

    json_object_put(root);
}
