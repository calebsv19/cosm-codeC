#include "core/Analysis/include_path_resolver.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

static char* dup_path(const char* p) {
    if (!p) return NULL;
    size_t len = strlen(p);
    if (len == 0) return NULL;
    char* out = malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, p, len);
    out[len] = '\0';
    return out;
}

static int add_path(char*** list, size_t* count, size_t* cap, const char* path) {
    if (!list || !count || !cap || !path || !*path) return 0;
    // Deduplicate
    for (size_t i = 0; i < *count; ++i) {
        if ((*list)[i] && strcmp((*list)[i], path) == 0) {
            return 1;
        }
    }
    if (*count >= *cap) {
        size_t newCap = (*cap == 0) ? 8 : (*cap * 2);
        char** tmp = realloc(*list, newCap * sizeof(char*));
        if (!tmp) return 0;
        *list = tmp;
        *cap = newCap;
    }
    (*list)[(*count)++] = dup_path(path);
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

static void parse_makefile_for_includes(const char* project_root,
                                        const char* makefile_path,
                                        char*** paths,
                                        size_t* count,
                                        size_t* cap) {
    FILE* f = fopen(makefile_path, "r");
    if (!f) return;

    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        // Very simple token scan: look for -I prefixes in tokens.
        const char* delim = " \t\r\n";
        char* save = NULL;
        for (char* tok = strtok_r(line, delim, &save); tok; tok = strtok_r(NULL, delim, &save)) {
            if (tok[0] == '-' && tok[1] == 'I') {
                const char* raw = tok + 2;
                if (*raw == '\0') continue;
                char expanded[PATH_MAX];
                expand_relative(project_root, raw, expanded, sizeof(expanded));
                add_path(paths, count, cap, expanded);
            }
        }
    }
    fclose(f);
}

size_t gather_include_paths(const char* project_root, char*** outPaths) {
    if (outPaths) *outPaths = NULL;
    char** list = NULL;
    size_t count = 0;
    size_t cap = 0;

    // 1) Parse Makefile/makefile if present.
    char mf[PATH_MAX];
    if (project_root && *project_root) {
        snprintf(mf, sizeof(mf), "%s/Makefile", project_root);
        parse_makefile_for_includes(project_root, mf, &list, &count, &cap);
        snprintf(mf, sizeof(mf), "%s/makefile", project_root);
        parse_makefile_for_includes(project_root, mf, &list, &count, &cap);
    }

    // 2) Add common defaults if missing.
    const char* defaults[] = {
        project_root,
        "src",
        "include",
        "/opt/homebrew/include",
        "/usr/local/include",
        "/opt/homebrew/opt/vulkan-loader/include",
        "../fisics/src"
    };
    for (size_t i = 0; i < sizeof(defaults)/sizeof(defaults[0]); ++i) {
        char expanded[PATH_MAX];
        if (i < 3) {
            // project-root relative for first entries
            expand_relative(project_root, defaults[i], expanded, sizeof(expanded));
        } else {
            strncpy(expanded, defaults[i], sizeof(expanded) - 1);
            expanded[sizeof(expanded) - 1] = '\0';
        }
        if (expanded[0]) {
            add_path(&list, &count, &cap, expanded);
        }
    }

    // 3) VULKAN_SDK env var if present.
    const char* vk = getenv("VULKAN_SDK");
    if (vk && *vk) {
        char expanded[PATH_MAX];
        snprintf(expanded, sizeof(expanded), "%s/include", vk);
        add_path(&list, &count, &cap, expanded);
    }

    if (outPaths) *outPaths = list;
    else free_include_paths(list, count);
    return count;
}

void free_include_paths(char** paths, size_t count) {
    if (!paths) return;
    for (size_t i = 0; i < count; ++i) {
        free(paths[i]);
    }
    free(paths);
}
