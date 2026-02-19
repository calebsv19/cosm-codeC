#include "core/Analysis/include_path_resolver.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <json-c/json.h>

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
                                                char* lastSysroot,
                                                size_t lastSysrootSize) {
    if (!flags) return;
    const char* delim = " \t\r\n";
    char buf[2048];
    size_t flen = strlen(flags);
    if (flen >= sizeof(buf)) flen = sizeof(buf) - 1;
    memcpy(buf, flags, flen);
    buf[flen] = '\0';

    char* save = NULL;
    for (char* tok = strtok_r(buf, delim, &save); tok; tok = strtok_r(NULL, delim, &save)) {
        if (strcmp(tok, "-I") == 0 || strcmp(tok, "-isystem") == 0) {
            char* next = strtok_r(NULL, delim, &save);
            if (!next) continue;
            char expanded[PATH_MAX];
            expand_relative(project_root, next, expanded, sizeof(expanded));
            add_unique(includes, icount, icap, expanded);
        } else if (strncmp(tok, "-I", 2) == 0) {
            const char* raw = tok + 2;
            if (*raw) {
                char expanded[PATH_MAX];
                expand_relative(project_root, raw, expanded, sizeof(expanded));
                add_unique(includes, icount, icap, expanded);
            }
        } else if (strncmp(tok, "-isystem", 8) == 0) {
            const char* raw = tok + 8;
            if (*raw) {
                char expanded[PATH_MAX];
                expand_relative(project_root, raw, expanded, sizeof(expanded));
                add_unique(includes, icount, icap, expanded);
            }
        } else if (strcmp(tok, "-D") == 0) {
            char* def = strtok_r(NULL, delim, &save);
            if (def && *def) add_unique(macros, mcount, mcap, def);
        } else if (strncmp(tok, "-D", 2) == 0) {
            const char* def = tok + 2;
            if (*def) add_unique(macros, mcount, mcap, def);
        } else if (strncmp(tok, "-isysroot", 9) == 0) {
            const char* root = tok + 9;
            if (!*root) {
                char* next = strtok_r(NULL, delim, &save);
                root = next;
            }
            if (root && *root) {
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
                                            lastSysroot, lastSysrootSize);
    }
    fclose(f);
}

// Run `make -pn` (or similar) to resolve variables without executing build steps.
static void parse_make_vars(const char* project_root,
                            char*** includes,
                            size_t* icount,
                            size_t* icap,
                            char*** macros,
                            size_t* mcount,
                            size_t* mcap,
                            char* lastSysroot,
                            size_t lastSysrootSize) {
    if (!project_root || !*project_root) return;
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "cd \"%s\" && make -pn 2>/dev/null", project_root);
    FILE* f = popen(cmd, "r");
    if (!f) return;

    const char* interesting[] = { "CPPFLAGS", "CFLAGS", "SDL2_CFLAGS", "SDLAPP_DIR", "SDKROOT" };
    char line[2048];
    while (fgets(line, sizeof(line), f)) {
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
                                                        lastSysroot, lastSysrootSize);
                }
            }
        }
    }
    pclose(f);
}

static void add_clang_resource_include(char*** includes,
                                       size_t* icount,
                                       size_t* icap) {
    const char* candidates[] = { "clang", "clang++" };
    char line[PATH_MAX];
    for (size_t i = 0; i < sizeof(candidates)/sizeof(candidates[0]); ++i) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "%s -print-resource-dir 2>/dev/null", candidates[i]);
        FILE* f = popen(cmd, "r");
        if (!f) continue;
        if (fgets(line, sizeof(line), f)) {
            // trim newline
            size_t len = strlen(line);
            while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
                line[--len] = '\0';
            }
            if (len > 0) {
                char inc[PATH_MAX];
                snprintf(inc, sizeof(inc), "%s/include", line);
                add_unique(includes, icount, icap, inc);
                pclose(f);
                return;
            }
        }
        pclose(f);
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
    }

    char** incs = NULL;
    char** defs = NULL;
    size_t icount = 0, dcount = 0;
    size_t icap = 0, dcap = 0;

    char lastSysroot[PATH_MAX] = {0};
    parse_makefiles(project_root, &incs, &icount, &icap, &defs, &dcount, &dcap,
                    lastSysroot, sizeof(lastSysroot));
    parse_make_vars(project_root, &incs, &icount, &icap, &defs, &dcount, &dcap,
                    lastSysroot, sizeof(lastSysroot));
    add_default_includes(project_root, &incs, &icount, &icap);
    parse_flags_for_includes_and_macros(project_root, extra_flags,
                                        &incs, &icount, &icap,
                                        &defs, &dcount, &dcap,
                                        lastSysroot, sizeof(lastSysroot));
    // If sysroot was captured, add frameworks root as well.
    if (lastSysroot[0]) {
        char fw[PATH_MAX];
        snprintf(fw, sizeof(fw), "%s/System/Library/Frameworks", lastSysroot);
        add_unique(&incs, &icount, &icap, fw);
    }
    add_clang_resource_include(&incs, &icount, &icap);

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
    }

    if (out) {
        out->include_paths = incs;
        out->include_count = icount;
        out->macro_defines = defs;
        out->macro_count = dcount;
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

// Persistence helpers -------------------------------------------------------
static void ensure_cache_dir(const char* workspace_root) {
    if (!workspace_root || !*workspace_root) return;
    char dir[PATH_MAX];
    snprintf(dir, sizeof(dir), "%s/ide_files", workspace_root);
    struct stat st;
    if (stat(dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
        mkdir(dir, 0755);
    }
}

void save_build_flags(const BuildFlagSet* set, const char* workspace_root) {
    if (!workspace_root || !*workspace_root || !set) return;
    ensure_cache_dir(workspace_root);

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

    const char* serialized = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/ide_files/build_flags.json", workspace_root);
    FILE* f = fopen(path, "w");
    if (f && serialized) {
        fputs(serialized, f);
        fclose(f);
    } else if (f) {
        fclose(f);
    }
    json_object_put(root);
}

void load_build_flags(BuildFlagSet* set, const char* workspace_root) {
    if (!set) return;
    free_build_flag_set(set);
    if (!workspace_root || !*workspace_root) return;

    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/ide_files/build_flags.json", workspace_root);
    FILE* f = fopen(path, "r");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0 || len > (32 * 1024 * 1024)) {
        fclose(f);
        return;
    }
    char* buf = malloc((size_t)len + 1);
    if (!buf) {
        fclose(f);
        return;
    }
    fread(buf, 1, (size_t)len, f);
    buf[len] = '\0';
    fclose(f);

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
            add_unique(&set->macro_defines, &set->macro_count, &dcap, v ? v : "");
        }
    }

    json_object_put(root);
}
