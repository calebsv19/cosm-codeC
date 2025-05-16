// file_watcher.h

#ifndef FILE_WATCHER_H
#define FILE_WATCHER_H

#include "Editor/editor_view.h"

void initFileWatcher();
void shutdownFileWatcher();

// Must be called every frame or second
void pollFileWatcher();

void watchFile(OpenFile* file);
void unwatchFile(OpenFile* file);

#endif

