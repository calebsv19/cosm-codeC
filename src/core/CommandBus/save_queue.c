#include "save_queue.h"
#include "ide/Panes/Editor/editor_buffer.h"
#include "ide/Panes/Editor/editor_view.h"
#include "core/Analysis/analysis_scheduler.h"
#include "core/Analysis/analysis_status.h"
#include "core/LoopEvents/event_queue.h"
#include "app/GlobalInfo/project.h"
#include "ide/Panes/ToolPanels/Git/tool_git.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>

static SaveQueueItem* head = NULL;
static SaveQueueItem* tail = NULL;

void initSaveQueue() {
    head = NULL;
    tail = NULL;
}

void shutdownSaveQueue() {
    // Free all remaining items
    while (head) {
        SaveQueueItem* next = head->next;
        free(head->filePath);
        free(head->contents);
        free(head);
        head = next;
    }
    tail = NULL;
}

void addSaveToQueue(const char* filePath, const char* contents, size_t length) {
    SaveQueueItem* item = malloc(sizeof(SaveQueueItem));
    item->filePath = strdup(filePath);
    item->contents = malloc(length);
    memcpy(item->contents, contents, length);
    item->length = length;
    item->next = NULL;

    if (!tail) {
        head = tail = item;
    } else {
        tail->next = item;
        tail = item;
    }

    printf("[SaveQueue] Queued save for: %s (%zu bytes)\n", filePath, length);
}

void queueSaveFromOpenFile(OpenFile* file) {
    if (!file || !file->filePath || !file->buffer) {
        printf("[SaveQueue] Invalid file passed to save\n");
        return;
    }

    size_t length = 0;
    char* snapshot = getBufferSnapshot(file->buffer, &length);
    
    if (!snapshot) {
        printf("[SaveQueue] Failed snapshot for: %s\n", file->filePath);
        return;
    }

    addSaveToQueue(file->filePath, snapshot, length);
    free(snapshot);

    file->isModified = false;
    file->showSavedTag = true;
    file->savedTagTimestamp = SDL_GetTicks();
}



void tickSaveQueue() {
    if (!head) return;

    SaveQueueItem* item = head;

    FILE* f = fopen(item->filePath, "w");
    if (f) {
        fwrite(item->contents, 1, item->length, f);
        fclose(f);
        printf("[SaveQueue] Saved: %s\n", item->filePath);

        // Queue async analysis refresh; do not block the main thread on save.
        analysis_status_set(ANALYSIS_STATUS_STALE_LOADING);
        loop_events_emit_analysis_status_updated(projectPath, 0u, SDL_GetTicks64());
        analysis_scheduler_request(ANALYSIS_REASON_PROJECT_MUTATION, false);
        resetGitTree();
    } else {
        printf("[SaveQueue] FAILED to save: %s\n", item->filePath);
    }

    head = item->next;
    if (!head) tail = NULL;

    free(item->filePath);
    free(item->contents);
    free(item);
}

bool isSaveQueueBusy() {
    return head != NULL;
}
