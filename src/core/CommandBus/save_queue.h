#ifndef SAVE_QUEUE_H
#define SAVE_QUEUE_H


#include "ide/Panes/Editor/editor_view.h"  

#include <stdbool.h>
#include <stddef.h>



void initSaveQueue();
void shutdownSaveQueue();
void tickSaveQueue();  // call this once per frame or main loop tick



// Public entry point
void addSaveToQueue(const char* filePath, const char* contents, size_t length);
bool queueSaveFromOpenFile(OpenFile* file);

bool finishPendingFileSaves(OpenFile* file);
bool isSaveQueueBusy(); // optional helper

#endif

