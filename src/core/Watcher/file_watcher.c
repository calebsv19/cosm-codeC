
#include "file_watcher.h"
#include "app/GlobalInfo/project.h"
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

typedef struct WatchedFile {
    OpenFile* file;
    time_t lastModified;
    struct WatchedFile* next;
} WatchedFile;

static WatchedFile* head = NULL;
static char watchedWorkspacePath[PATH_MAX];
static time_t workspaceLastModified = 0;

void initFileWatcher() {
    head = NULL;
    watchedWorkspacePath[0] = '\0';
    workspaceLastModified = 0;
}

void shutdownFileWatcher() {
    while (head) {
        WatchedFile* next = head->next;
        free(head);
        head = next;
    }
    watchedWorkspacePath[0] = '\0';
    workspaceLastModified = 0;
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

void setWorkspaceWatchPath(const char* path) {
    if (!path || path[0] == '\0') {
        watchedWorkspacePath[0] = '\0';
        workspaceLastModified = 0;
        return;
    }

    strncpy(watchedWorkspacePath, path, sizeof(watchedWorkspacePath) - 1);
    watchedWorkspacePath[sizeof(watchedWorkspacePath) - 1] = '\0';
    workspaceLastModified = 0;
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

    if (watchedWorkspacePath[0] != '\0') {
        struct stat dirStat;
        if (stat(watchedWorkspacePath, &dirStat) == 0) {
            if (workspaceLastModified == 0) {
                workspaceLastModified = dirStat.st_mtime;
            } else if (dirStat.st_mtime != workspaceLastModified) {
                workspaceLastModified = dirStat.st_mtime;
                pendingProjectRefresh = true;
            }
        }
    }
}
