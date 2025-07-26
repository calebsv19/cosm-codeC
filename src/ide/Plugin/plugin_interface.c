#include "plugin_interface.h"

#include <stdio.h>
#include <string.h>

#if defined(__linux__) || defined(__APPLE__)
#include <dlfcn.h>
#elif defined(_WIN32)
#include <windows.h>
#else
#error "Unsupported platform"
#endif

#define MAX_PLUGINS 64

static Plugin plugins[MAX_PLUGINS];
static int pluginCount = 0;

void initPluginSystem() {
    pluginCount = 0;
}

bool loadPlugin(const char* path) {
    if (pluginCount >= MAX_PLUGINS) return false;

    void* handle = NULL;

#if defined(__linux__) || defined(__APPLE__)
    handle = dlopen(path, RTLD_NOW);
    if (!handle) {
        fprintf(stderr, "Failed to load plugin %s: %s\n", path, dlerror());
        return false;
    }
#elif defined(_WIN32)
    handle = LoadLibraryA(path);
    if (!handle) {
        fprintf(stderr, "Failed to load plugin %s\n", path);
        return false;
    }
#endif

    Plugin* plugin = &plugins[pluginCount++];
    plugin->name = strdup(path);  // In future, query plugin metadata
    plugin->version = "0.1";
    plugin->description = "No description available.";
    plugin->loaded = true;
    plugin->handle = handle;

    return true;
}

void unloadAllPlugins() {
    for (int i = 0; i < pluginCount; i++) {
#if defined(__linux__) || defined(__APPLE__)
        if (plugins[i].handle) dlclose(plugins[i].handle);
#elif defined(_WIN32)
        if (plugins[i].handle) FreeLibrary((HMODULE)plugins[i].handle);
#endif
        plugins[i].loaded = false;
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

