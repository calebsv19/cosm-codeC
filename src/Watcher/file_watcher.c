
#include "file_watcher.h"
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct WatchedFile {
    OpenFile* file;
    time_t lastModified;
    struct WatchedFile* next;
} WatchedFile;

static WatchedFile* head = NULL;

void initFileWatcher() {
    head = NULL;
}

void shutdownFileWatcher() {
    while (head) {
        WatchedFile* next = head->next;
        free(head);
        head = next;
    }
}

void watchFile(OpenFile* file) {
    struct stat st;
    if (stat(file->filePath, &st) != 0) return;

    WatchedFile* wf = malloc(sizeof(WatchedFile));
    wf->file = file;
    wf->lastModified = st.st_mtime;
    wf->next = head;
    head = wf;
}

void unwatchFile(OpenFile* file) {
    WatchedFile** ptr = &head;
    while (*ptr) {
        if ((*ptr)->file == file) {
            WatchedFile* toRemove = *ptr;
            *ptr = (*ptr)->next;
            free(toRemove);
            return;
        }
        ptr = &((*ptr)->next);
    }
}

void pollFileWatcher() {
    WatchedFile* current = head;
    while (current) {
        struct stat st;
        if (stat(current->file->filePath, &st) == 0) {
            if (st.st_mtime != current->lastModified) {
                if (!current->file->isModified) {
                    printf("[FileWatcher] Auto-reloading %s\n", current->file->filePath);
                    reloadOpenFileFromDisk(current->file);
                    current->lastModified = st.st_mtime;
                } else {
                    printf("[FileWatcher] WARNING: %s changed on disk but has unsaved changes.\n", 
				current->file->filePath);
                }
            }
        }
        current = current->next;
    }
}

