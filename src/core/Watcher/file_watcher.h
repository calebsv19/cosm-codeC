// file_watcher.h

#ifndef FILE_WATCHER_H
#define FILE_WATCHER_H

#include "ide/Panes/Editor/editor_view.h"
#include <SDL2/SDL.h>

void initFileWatcher();
void shutdownFileWatcher();

// Poll watcher state on scheduler tick.
void pollFileWatcher();
Uint32 fileWatcherPollIntervalMs(void);

void watchFile(OpenFile* file);
void unwatchFile(OpenFile* file);
void setWorkspaceWatchPath(const char* path);
// Temporarily suppress watcher-triggered workspace refresh events.
void suppressWorkspaceWatchRefreshForMs(unsigned int durationMs);
// Suppress workspace refresh triggers during internal IDE write bursts
// (analysis persistence/cache writes).
void suppressInternalWatcherRefreshForMs(unsigned int durationMs);

#endif
