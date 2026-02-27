#include "plugin_interface.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <json-c/json.h>

#if defined(__linux__) || defined(__APPLE__)
#include <dlfcn.h>
#elif defined(_WIN32)
#include <windows.h>
#else
#error "Unsupported platform"
#endif

#define MAX_PLUGINS 64

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

extern const char* getWorkspacePath(void);

static Plugin plugins[MAX_PLUGINS];
static int pluginCount = 0;

typedef struct PluginManifest {
    char* name;
    char* version;
    char* description;
    char* capabilities_csv;
} PluginManifest;

static bool strings_equal_ci(const char* a, const char* b) {
    if (!a || !b) return false;
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return false;
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static bool env_flag_enabled(const char* name) {
    const char* v = getenv(name);
    if (!v || !v[0]) return false;
    return strcmp(v, "1") == 0 || strings_equal_ci(v, "true") || strings_equal_ci(v, "yes") ||
           strings_equal_ci(v, "on");
}

static bool is_absolute_path(const char* path) {
    if (!path || !path[0]) return false;
#if defined(_WIN32)
    if ((isalpha((unsigned char)path[0]) && path[1] == ':' && (path[2] == '\\' || path[2] == '/')) ||
        path[0] == '\\' || path[0] == '/') {
        return true;
    }
    return false;
#else
    return path[0] == '/';
#endif
}

static bool canonicalize_existing_path(const char* path, char* out_abs, size_t out_abs_cap) {
    if (!path || !path[0] || !out_abs || out_abs_cap == 0) return false;
#if defined(_WIN32)
    if (!_fullpath(out_abs, path, out_abs_cap)) return false;
    return out_abs[0] != '\0';
#else
    return realpath(path, out_abs) != NULL;
#endif
}

static void join_paths(const char* left, const char* right, char* out, size_t out_cap) {
    if (!out || out_cap == 0) return;
    out[0] = '\0';
    if (!left || !left[0]) {
        snprintf(out, out_cap, "%s", right ? right : "");
        return;
    }
    if (!right || !right[0]) {
        snprintf(out, out_cap, "%s", left);
        return;
    }
    const size_t n = strlen(left);
    const bool need_sep = left[n - 1] != '/' && left[n - 1] != '\\';
    snprintf(out, out_cap, "%s%s%s", left, need_sep ? "/" : "", right);
}

static char* trim_in_place(char* s) {
    if (!s) return s;
    while (*s && isspace((unsigned char)*s)) s++;
    char* end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) end--;
    *end = '\0';
    return s;
}

static bool path_is_dir(const char* path_abs) {
    struct stat st;
    if (!path_abs || stat(path_abs, &st) != 0) return false;
    return S_ISDIR(st.st_mode);
}

static bool path_has_prefix_boundary(const char* full, const char* prefix) {
    if (!full || !prefix) return false;
    const size_t n = strlen(prefix);
    if (strncmp(full, prefix, n) != 0) return false;
    return full[n] == '\0' || full[n] == '/' || full[n] == '\\';
}

static bool allowlist_entry_matches_plugin(const char* plugin_abs, const char* entry_raw) {
    if (!plugin_abs || !entry_raw || !entry_raw[0]) return false;

    char candidate[PATH_MAX];
    if (is_absolute_path(entry_raw)) {
        snprintf(candidate, sizeof(candidate), "%s", entry_raw);
    } else {
        const char* workspace = getWorkspacePath();
        if (!workspace || !workspace[0]) return false;
        join_paths(workspace, entry_raw, candidate, sizeof(candidate));
    }

    char entry_abs[PATH_MAX];
    if (!canonicalize_existing_path(candidate, entry_abs, sizeof(entry_abs))) return false;

    if (path_is_dir(entry_abs)) {
        return path_has_prefix_boundary(plugin_abs, entry_abs);
    }
    return strcmp(plugin_abs, entry_abs) == 0;
}

static bool plugin_allowlisted_by_env(const char* plugin_abs) {
    const char* raw = getenv("IDE_PLUGIN_ALLOWLIST");
    if (!raw || !raw[0]) return false;

    char buf[8192];
    snprintf(buf, sizeof(buf), "%s", raw);

#if defined(_WIN32)
    const char* delim = ";";
#else
    const char* delim = ":";
#endif
    char* token = strtok(buf, delim);
    while (token) {
        char* entry = trim_in_place(token);
        if (entry[0] && allowlist_entry_matches_plugin(plugin_abs, entry)) return true;
        token = strtok(NULL, delim);
    }
    return false;
}

static bool resolve_allowlist_file_path(char* out_path, size_t out_path_cap) {
    if (!out_path || out_path_cap == 0) return false;
    out_path[0] = '\0';

    const char* override = getenv("IDE_PLUGIN_ALLOWLIST_FILE");
    if (override && override[0]) {
        snprintf(out_path, out_path_cap, "%s", override);
        return true;
    }

    const char* workspace = getWorkspacePath();
    if (!workspace || !workspace[0]) return false;
    join_paths(workspace, "ide_files/plugin_allowlist.txt", out_path, out_path_cap);
    return true;
}

static bool plugin_allowlisted_by_file(const char* plugin_abs) {
    char allowlist_path[PATH_MAX];
    if (!resolve_allowlist_file_path(allowlist_path, sizeof(allowlist_path))) return false;

    FILE* f = fopen(allowlist_path, "r");
    if (!f) return false;

    bool matched = false;
    char line[PATH_MAX + 128];
    while (fgets(line, sizeof(line), f)) {
        char* entry = trim_in_place(line);
        if (!entry[0] || entry[0] == '#') continue;
        if (allowlist_entry_matches_plugin(plugin_abs, entry)) {
            matched = true;
            break;
        }
    }

    fclose(f);
    return matched;
}

static bool plugin_path_is_allowlisted(const char* plugin_abs) {
    return plugin_allowlisted_by_env(plugin_abs) || plugin_allowlisted_by_file(plugin_abs);
}

static void clear_manifest(PluginManifest* manifest) {
    if (!manifest) return;
    free(manifest->name);
    free(manifest->version);
    free(manifest->description);
    free(manifest->capabilities_csv);
    memset(manifest, 0, sizeof(*manifest));
}

static char* capabilities_to_csv(json_object* caps) {
    if (!caps || !json_object_is_type(caps, json_type_array)) return NULL;

    char* csv = strdup("");
    if (!csv) return NULL;

    const size_t n = json_object_array_length(caps);
    size_t len = 0;
    for (size_t i = 0; i < n; i++) {
        json_object* item = json_object_array_get_idx(caps, i);
        if (!item || !json_object_is_type(item, json_type_string)) {
            free(csv);
            return NULL;
        }
        const char* text = json_object_get_string(item);
        if (!text) {
            free(csv);
            return NULL;
        }
        const size_t item_len = strlen(text);
        const size_t need = len + (len ? 1 : 0) + item_len + 1;
        char* next = realloc(csv, need);
        if (!next) {
            free(csv);
            return NULL;
        }
        csv = next;
        if (len) csv[len++] = ',';
        memcpy(csv + len, text, item_len);
        len += item_len;
        csv[len] = '\0';
    }

    return csv;
}

static bool load_manifest_if_present(const char* plugin_abs,
                                     PluginManifest* out_manifest,
                                     char* error_out,
                                     size_t error_out_cap) {
    if (!plugin_abs || !out_manifest) return false;

    char manifest_path[PATH_MAX];
    snprintf(manifest_path, sizeof(manifest_path), "%s.manifest.json", plugin_abs);

    FILE* probe = fopen(manifest_path, "r");
    if (!probe) {
        if (env_flag_enabled("IDE_PLUGIN_REQUIRE_MANIFEST")) {
            if (error_out && error_out_cap > 0) {
                snprintf(error_out, error_out_cap, "manifest required but missing: %s", manifest_path);
            }
            return false;
        }
        return true;
    }
    fclose(probe);

    json_object* root = json_object_from_file(manifest_path);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (error_out && error_out_cap > 0) {
            snprintf(error_out, error_out_cap, "invalid manifest JSON: %s", manifest_path);
        }
        if (root) json_object_put(root);
        return false;
    }

    json_object *jname = NULL, *jversion = NULL, *jcaps = NULL, *jdescription = NULL;
    if (!json_object_object_get_ex(root, "name", &jname) || !jname ||
        !json_object_is_type(jname, json_type_string) ||
        json_object_get_string(jname)[0] == '\0') {
        if (error_out && error_out_cap > 0) {
            snprintf(error_out, error_out_cap, "manifest missing string field: name");
        }
        json_object_put(root);
        return false;
    }
    if (!json_object_object_get_ex(root, "version", &jversion) || !jversion ||
        !json_object_is_type(jversion, json_type_string) ||
        json_object_get_string(jversion)[0] == '\0') {
        if (error_out && error_out_cap > 0) {
            snprintf(error_out, error_out_cap, "manifest missing string field: version");
        }
        json_object_put(root);
        return false;
    }
    if (!json_object_object_get_ex(root, "capabilities", &jcaps) || !jcaps ||
        !json_object_is_type(jcaps, json_type_array)) {
        if (error_out && error_out_cap > 0) {
            snprintf(error_out, error_out_cap, "manifest missing array field: capabilities");
        }
        json_object_put(root);
        return false;
    }

    if (json_object_object_get_ex(root, "description", &jdescription) &&
        jdescription && !json_object_is_type(jdescription, json_type_string)) {
        if (error_out && error_out_cap > 0) {
            snprintf(error_out, error_out_cap, "manifest description must be a string");
        }
        json_object_put(root);
        return false;
    }

    out_manifest->name = strdup(json_object_get_string(jname));
    out_manifest->version = strdup(json_object_get_string(jversion));
    out_manifest->capabilities_csv = capabilities_to_csv(jcaps);
    out_manifest->description =
        strdup((jdescription && json_object_is_type(jdescription, json_type_string))
                   ? json_object_get_string(jdescription)
                   : "No description available.");

    const bool ok = out_manifest->name && out_manifest->version && out_manifest->capabilities_csv &&
                    out_manifest->description;
    if (!ok) {
        if (error_out && error_out_cap > 0) {
            snprintf(error_out, error_out_cap, "manifest allocation failed");
        }
        clear_manifest(out_manifest);
        json_object_put(root);
        return false;
    }

    json_object_put(root);
    return true;
}

static void clear_plugin_entry(Plugin* plugin) {
    if (!plugin) return;
    free(plugin->name);
    free(plugin->version);
    free(plugin->description);
    free(plugin->capabilities);
    memset(plugin, 0, sizeof(*plugin));
}

static const char* plugin_basename(const char* path) {
    if (!path) return "";
    const char* slash = strrchr(path, '/');
    const char* bslash = strrchr(path, '\\');
    const char* sep = slash;
    if (!sep || (bslash && bslash > sep)) sep = bslash;
    return sep ? (sep + 1) : path;
}

void initPluginSystem() {
    unloadAllPlugins();
}

bool loadPlugin(const char* path) {
    if (!path || !path[0]) return false;
    if (pluginCount >= MAX_PLUGINS) return false;

    if (!env_flag_enabled("IDE_ENABLE_PLUGINS")) {
        fprintf(stderr,
                "[Plugin] Refused to load '%s': plugin loading is disabled. "
                "Set IDE_ENABLE_PLUGINS=1 and allowlist the plugin path.\n",
                path);
        return false;
    }

    char plugin_abs[PATH_MAX];
    if (!canonicalize_existing_path(path, plugin_abs, sizeof(plugin_abs))) {
        fprintf(stderr, "[Plugin] Refused to load '%s': failed to resolve canonical path.\n", path);
        return false;
    }

    if (!plugin_path_is_allowlisted(plugin_abs)) {
        fprintf(stderr,
                "[Plugin] Refused to load '%s': path is not in plugin allowlist. "
                "Use IDE_PLUGIN_ALLOWLIST, IDE_PLUGIN_ALLOWLIST_FILE, or workspace "
                "ide_files/plugin_allowlist.txt.\n",
                plugin_abs);
        return false;
    }

    PluginManifest manifest = {0};
    char manifest_err[256] = {0};
    if (!load_manifest_if_present(plugin_abs, &manifest, manifest_err, sizeof(manifest_err))) {
        fprintf(stderr, "[Plugin] Refused to load '%s': %s\n", plugin_abs,
                manifest_err[0] ? manifest_err : "manifest validation failed");
        clear_manifest(&manifest);
        return false;
    }

    void* handle = NULL;

#if defined(__linux__) || defined(__APPLE__)
    handle = dlopen(plugin_abs, RTLD_NOW);
    if (!handle) {
        fprintf(stderr, "Failed to load plugin %s: %s\n", plugin_abs, dlerror());
        clear_manifest(&manifest);
        return false;
    }
#elif defined(_WIN32)
    handle = LoadLibraryA(plugin_abs);
    if (!handle) {
        fprintf(stderr, "Failed to load plugin %s\n", plugin_abs);
        clear_manifest(&manifest);
        return false;
    }
#endif

    Plugin* plugin = &plugins[pluginCount++];
    plugin->name = manifest.name ? manifest.name : strdup(plugin_basename(plugin_abs));
    plugin->version = manifest.version ? manifest.version : strdup("0.1");
    plugin->description =
        manifest.description ? manifest.description : strdup("No description available.");
    plugin->capabilities = manifest.capabilities_csv ? manifest.capabilities_csv : strdup("");
    plugin->loaded = true;
    plugin->handle = handle;

    manifest.name = NULL;
    manifest.version = NULL;
    manifest.description = NULL;
    manifest.capabilities_csv = NULL;
    clear_manifest(&manifest);

    if (!plugin->name || !plugin->version || !plugin->description || !plugin->capabilities) {
#if defined(__linux__) || defined(__APPLE__)
        if (plugin->handle) dlclose(plugin->handle);
#elif defined(_WIN32)
        if (plugin->handle) FreeLibrary((HMODULE)plugin->handle);
#endif
        clear_plugin_entry(plugin);
        pluginCount--;
        return false;
    }

    return true;
}

void unloadAllPlugins() {
    for (int i = 0; i < pluginCount; i++) {
#if defined(__linux__) || defined(__APPLE__)
        if (plugins[i].handle) dlclose(plugins[i].handle);
#elif defined(_WIN32)
        if (plugins[i].handle) FreeLibrary((HMODULE)plugins[i].handle);
#endif
        clear_plugin_entry(&plugins[i]);
    }
    pluginCount = 0;
}

int getLoadedPluginCount() {
    return pluginCount;
}

const Plugin* getPluginAt(int index) {
    if (index < 0 || index >= pluginCount) return NULL;
    return &plugins[index];
}
