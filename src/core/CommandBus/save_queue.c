#include "save_queue.h"
#include "save_publication.h"
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

typedef struct SaveQueueItem {
    char* filePath;
    char* contents;
    size_t length;
    OpenFile* owner; /* Drained before the last editor reference is released. */
    uint64_t revision, contentRevision;
    struct SaveQueueItem* next;
} SaveQueueItem;

static SaveQueueItem* head = NULL;
static SaveQueueItem* tail = NULL;

void initSaveQueue() {
    head = NULL;
    tail = NULL;
}

void shutdownSaveQueue() {
    while (head) tickSaveQueue();
}

static bool enqueueSnapshot(const char* path, char* contents, size_t length, OpenFile* owner) {
    SaveQueueItem* item = calloc(1, sizeof(*item));
    if (!item || !(item->filePath = strdup(path))) {
        free(item);
        free(contents);
        return false;
    }
    item->contents = contents;
    item->length = length;
    item->owner = owner;
    item->revision = owner ? owner->documentRevision : 0;
    item->contentRevision = owner ? owner->contentRevision : 0;
    if (tail) tail->next = item;
    else head = item;
    tail = item;
    return true;
}

void addSaveToQueue(const char* path, const char* contents, size_t length) {
    if (!path || !contents) return;
    char* copy = malloc(length ? length : 1);
    if (copy) memcpy(copy, contents, length);
    if (!copy || !enqueueSnapshot(path, copy, length, NULL))
        fprintf(stderr, "[SaveQueue] Could not queue %s\n", path);
}

bool queueSaveFromOpenFile(OpenFile* file) {
    if (!file || !file->filePath || !file->buffer) return false;
    size_t length = 0;
    char* snapshot = getBufferSnapshot(file->buffer, &length);
    file->showSavedTag = false;
    file->saveFailed = !snapshot || !enqueueSnapshot(file->filePath, snapshot, length, file);
    if (file->saveFailed) file->isModified = true;
    if (!file->saveFailed) file->pendingTextEdit = false;
    return !file->saveFailed;
}

void tickSaveQueue() {
    if (!head) return;
    SaveQueueItem* item = head;
    OpenFile* file = item->owner;
    if (publishEditorSnapshot(item->filePath, item->contents, item->length)) {
        if (file && !strcmp(item->filePath, file->filePath)) file->savedContentRevision = item->contentRevision;
        if (file && !strcmp(item->filePath, file->filePath) && item->revision == file->documentRevision) {
            file->saveFailed = false;
            file->isModified = false;
            file->showSavedTag = true;
            file->savedTagTimestamp = SDL_GetTicks();
        }
        // Queue async analysis refresh; do not block the main thread on save.
        analysis_status_set(ANALYSIS_STATUS_STALE_LOADING);
        loop_events_emit_analysis_status_updated(projectPath, 0u, SDL_GetTicks64());
        analysis_scheduler_request_file(item->filePath,
                                        ANALYSIS_REASON_EDITOR_SAVE,
                                        false);
        // Let the git watcher decide whether the panel needs a refresh so
        // save-driven analysis churn does not force tree/UI resets.
        pollGitStatusWatcher();
    } else {
        if (file) {
            file->saveFailed = true;
            file->isModified = true;
            file->showSavedTag = false;
        }
        fprintf(stderr, "[SaveQueue] FAILED to save: %s\n", item->filePath);
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

bool finishPendingFileSaves(OpenFile* file) {
    /* Preserve FIFO order across duplicate paths before a file can be freed. */
    SaveQueueItem* last = NULL;
    for (SaveQueueItem* item = head; item; item = item->next)
        if (item->owner == file) last = item;
    if (last) {
        while (head != last) tickSaveQueue();
        tickSaveQueue();
        if (file->isModified) return false;
    }
    return !file->saveFailed;
}
