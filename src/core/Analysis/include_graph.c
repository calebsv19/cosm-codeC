#include "core/Analysis/include_graph.h"

#include <json-c/json.h>
#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

typedef struct {
    char* source_path;
    char** deps;
    size_t dep_count;
    size_t dep_capacity;
} IncludeEdgeEntry;

static IncludeEdgeEntry* g_entries = NULL;
static size_t g_entry_count = 0;
static size_t g_entry_capacity = 0;
static pthread_mutex_t g_include_graph_mutex = PTHREAD_MUTEX_INITIALIZER;

static void include_graph_lock(void) {
    pthread_mutex_lock(&g_include_graph_mutex);
}

static void include_graph_unlock(void) {
    pthread_mutex_unlock(&g_include_graph_mutex);
}

static void free_entry(IncludeEdgeEntry* entry) {
    if (!entry) return;
    free(entry->source_path);
    for (size_t i = 0; i < entry->dep_count; ++i) {
        free(entry->deps[i]);
    }
    free(entry->deps);
    memset(entry, 0, sizeof(*entry));
}

static int find_entry_index_locked(const char* source_path) {
    if (!source_path) return -1;
    for (size_t i = 0; i < g_entry_count; ++i) {
        if (g_entries[i].source_path && strcmp(g_entries[i].source_path, source_path) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static bool entry_has_dep(const IncludeEdgeEntry* entry, const char* dep) {
    if (!entry || !dep) return false;
    for (size_t i = 0; i < entry->dep_count; ++i) {
        if (entry->deps[i] && strcmp(entry->deps[i], dep) == 0) return true;
    }
    return false;
}

static bool entry_add_dep(IncludeEdgeEntry* entry, const char* dep) {
    if (!entry || !dep || !*dep) return false;
    if (entry_has_dep(entry, dep)) return true;
    if (entry->dep_count >= entry->dep_capacity) {
        size_t new_cap = entry->dep_capacity ? entry->dep_capacity * 2 : 8;
        char** grown = (char**)realloc(entry->deps, new_cap * sizeof(char*));
        if (!grown) return false;
        entry->deps = grown;
        entry->dep_capacity = new_cap;
    }
    entry->deps[entry->dep_count] = strdup(dep);
    if (!entry->deps[entry->dep_count]) return false;
    entry->dep_count++;
    return true;
}

static bool ensure_entries_capacity(size_t add_count) {
    if (g_entry_count + add_count <= g_entry_capacity) return true;
    size_t new_cap = g_entry_capacity ? g_entry_capacity * 2 : 16;
    while (new_cap < g_entry_count + add_count) {
        new_cap *= 2;
    }
    IncludeEdgeEntry* grown =
        (IncludeEdgeEntry*)realloc(g_entries, new_cap * sizeof(IncludeEdgeEntry));
    if (!grown) return false;
    g_entries = grown;
    g_entry_capacity = new_cap;
    return true;
}

static bool is_under_root(const char* path, const char* root) {
    if (!path || !root || !*path || !*root) return false;
    size_t root_len = strlen(root);
    if (strncmp(path, root, root_len) != 0) return false;
    return path[root_len] == '/' || path[root_len] == '\\' || path[root_len] == '\0';
}

void include_graph_clear(void) {
    include_graph_lock();
    for (size_t i = 0; i < g_entry_count; ++i) {
        free_entry(&g_entries[i]);
    }
    free(g_entries);
    g_entries = NULL;
    g_entry_count = 0;
    g_entry_capacity = 0;
    include_graph_unlock();
}

void include_graph_remove_source(const char* source_path) {
    if (!source_path || !*source_path) return;
    include_graph_lock();
    int idx = find_entry_index_locked(source_path);
    if (idx >= 0) {
        free_entry(&g_entries[idx]);
        for (size_t i = (size_t)idx + 1; i < g_entry_count; ++i) {
            g_entries[i - 1] = g_entries[i];
        }
        g_entry_count--;
    }
    include_graph_unlock();
}

void include_graph_replace_from_result(const char* source_path,
                                       const FisicsInclude* includes,
                                       size_t include_count,
                                       const char* workspace_root) {
    if (!source_path || !*source_path) return;
    include_graph_lock();

    int idx = find_entry_index_locked(source_path);
    if (idx >= 0) {
        free_entry(&g_entries[idx]);
        for (size_t i = (size_t)idx + 1; i < g_entry_count; ++i) {
            g_entries[i - 1] = g_entries[i];
        }
        g_entry_count--;
    }

    if (!ensure_entries_capacity(1)) {
        include_graph_unlock();
        return;
    }
    IncludeEdgeEntry* entry = &g_entries[g_entry_count++];
    memset(entry, 0, sizeof(*entry));
    entry->source_path = strdup(source_path);
    if (!entry->source_path) {
        g_entry_count--;
        include_graph_unlock();
        return;
    }

    for (size_t i = 0; i < include_count; ++i) {
        const FisicsInclude* inc = &includes[i];
        const char* dep = NULL;
        if (inc->resolved_path && *inc->resolved_path && is_under_root(inc->resolved_path, workspace_root)) {
            dep = inc->resolved_path;
        }
        if (!dep) continue;
        (void)entry_add_dep(entry, dep);
    }

    include_graph_unlock();
}

size_t include_graph_collect_dependents(const char* changed_path, char*** out_paths) {
    if (!changed_path || !*changed_path || !out_paths) return 0;
    *out_paths = NULL;
    include_graph_lock();

    size_t out_count = 0;
    size_t out_capacity = 0;
    char** out = NULL;
    for (size_t i = 0; i < g_entry_count; ++i) {
        const IncludeEdgeEntry* entry = &g_entries[i];
        if (!entry->source_path) continue;
        if (strcmp(entry->source_path, changed_path) == 0) continue;
        if (!entry_has_dep(entry, changed_path)) continue;
        if (out_count >= out_capacity) {
            size_t new_cap = out_capacity ? out_capacity * 2 : 16;
            char** grown = (char**)realloc(out, new_cap * sizeof(char*));
            if (!grown) break;
            out = grown;
            out_capacity = new_cap;
        }
        out[out_count] = strdup(entry->source_path);
        if (!out[out_count]) break;
        out_count++;
    }

    include_graph_unlock();
    *out_paths = out;
    return out_count;
}

void include_graph_free_path_list(char** paths, size_t count) {
    if (!paths) return;
    for (size_t i = 0; i < count; ++i) {
        free(paths[i]);
    }
    free(paths);
}

static void ensure_ide_dir(const char* workspace_root) {
    if (!workspace_root || !*workspace_root) return;
    char dir[PATH_MAX];
    snprintf(dir, sizeof(dir), "%s/ide_files", workspace_root);
    struct stat st;
    if (stat(dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
        mkdir(dir, 0755);
    }
}

void include_graph_save(const char* workspace_root) {
    if (!workspace_root || !*workspace_root) return;
    ensure_ide_dir(workspace_root);
    include_graph_lock();

    json_object* root = json_object_new_object();
    json_object* entries = json_object_new_array();
    for (size_t i = 0; i < g_entry_count; ++i) {
        const IncludeEdgeEntry* e = &g_entries[i];
        json_object* obj = json_object_new_object();
        json_object_object_add(obj, "source", json_object_new_string(e->source_path ? e->source_path : ""));
        json_object* deps = json_object_new_array();
        for (size_t d = 0; d < e->dep_count; ++d) {
            json_object_array_add(deps, json_object_new_string(e->deps[d] ? e->deps[d] : ""));
        }
        json_object_object_add(obj, "deps", deps);
        json_object_array_add(entries, obj);
    }
    json_object_object_add(root, "entries", entries);

    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/ide_files/include_graph.json", workspace_root);
    FILE* fp = fopen(path, "w");
    if (fp) {
        const char* serialized = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
        if (serialized) fputs(serialized, fp);
        fclose(fp);
    }
    json_object_put(root);
    include_graph_unlock();
}

void include_graph_load(const char* workspace_root) {
    include_graph_clear();
    if (!workspace_root || !*workspace_root) return;

    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/ide_files/include_graph.json", workspace_root);
    FILE* fp = fopen(path, "r");
    if (!fp) return;

    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (len <= 0 || len > (32 * 1024 * 1024)) {
        fclose(fp);
        return;
    }

    char* buf = (char*)malloc((size_t)len + 1);
    if (!buf) {
        fclose(fp);
        return;
    }
    fread(buf, 1, (size_t)len, fp);
    buf[len] = '\0';
    fclose(fp);

    json_object* root = json_tokener_parse(buf);
    free(buf);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        return;
    }

    json_object* entries = NULL;
    if (!json_object_object_get_ex(root, "entries", &entries) ||
        !entries || !json_object_is_type(entries, json_type_array)) {
        json_object_put(root);
        return;
    }

    include_graph_lock();
    size_t count = json_object_array_length(entries);
    for (size_t i = 0; i < count; ++i) {
        json_object* obj = json_object_array_get_idx(entries, i);
        if (!obj || !json_object_is_type(obj, json_type_object)) continue;
        json_object* jsource = NULL;
        json_object* jdeps = NULL;
        json_object_object_get_ex(obj, "source", &jsource);
        json_object_object_get_ex(obj, "deps", &jdeps);
        const char* source = jsource ? json_object_get_string(jsource) : NULL;
        if (!source || !*source) continue;

        if (!ensure_entries_capacity(1)) continue;
        IncludeEdgeEntry* entry = &g_entries[g_entry_count++];
        memset(entry, 0, sizeof(*entry));
        entry->source_path = strdup(source);
        if (!entry->source_path) {
            g_entry_count--;
            continue;
        }

        if (jdeps && json_object_is_type(jdeps, json_type_array)) {
            size_t dcount = json_object_array_length(jdeps);
            for (size_t d = 0; d < dcount; ++d) {
                json_object* jd = json_object_array_get_idx(jdeps, d);
                const char* dep = jd ? json_object_get_string(jd) : NULL;
                if (!dep || !*dep) continue;
                (void)entry_add_dep(entry, dep);
            }
        }
    }
    include_graph_unlock();
    json_object_put(root);
}
