#ifndef PLUGIN_INTERFACE_H
#define PLUGIN_INTERFACE_H

#include <stdbool.h>

typedef struct Plugin {
    char* name;
    char* version;
    char* description;
    char* capabilities;  // Comma-separated declared capabilities from manifest.
    bool loaded;
    void* handle;  // For dynamic library handle (e.g. dlopen or LoadLibrary)
} Plugin;

void initPluginSystem();
bool loadPlugin(const char* path);
void unloadAllPlugins();
int getLoadedPluginCount();
const Plugin* getPluginAt(int index);

#endif
