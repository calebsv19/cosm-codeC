#ifndef SAVE_QUEUE_H
#define SAVE_QUEUE_H


#include "../Editor/editor_view.h"  

#include <stdbool.h>
#include <stddef.h>



typedef struct SaveQueueItem {
    char* filePath;
    char* contents;
    size_t length;
    struct SaveQueueItem* next;
} SaveQueueItem;

void initSaveQueue();
void shutdownSaveQueue();
void tickSaveQueue();  // call this once per frame or main loop tick



// Public entry point
void addSaveToQueue(const char* filePath, const char* contents, size_t length);
void queueSaveFromOpenFile(OpenFile* file);

bool isSaveQueueBusy(); // optional helper

#endif

