
#include "file_watcher.h"
#include "app/GlobalInfo/project.h"
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <limits.h>
#include <SDL2/SDL.h>

typedef struct WatchedFile {
    OpenFile* file;
    time_t lastModified;
    struct WatchedFile* next;
} WatchedFile;

static WatchedFile* head = NULL;
static char watchedWorkspacePath[PATH_MAX];
static time_t workspaceLastModified = 0;
static Uint32 nextPollMs = 0;
static Uint32 pollIntervalMs = 250;
static int watcherLogEnabled = -1;

static int file_watcher_log_enabled(void) {
    if (watcherLogEnabled >= 0) return watcherLogEnabled;
    const char* env = getenv("IDE_FILE_WATCHER_LOG");
    watcherLogEnabled =
        (env && env[0] && (strcmp(env, "1") == 0 || strcmp(env, "true") == 0)) ? 1 : 0;
    return watcherLogEnabled;
}

static Uint32 file_watcher_poll_interval_ms(void) {
    const char* env = getenv("IDE_FILE_WATCHER_POLL_MS");
    if (!env || !env[0]) return pollIntervalMs;
    char* end = NULL;
    long v = strtol(env, &end, 10);
    if (end == env || v < 50 || v > 5000) return pollIntervalMs;
    return (Uint32)v;
}

static time_t compute_workspace_stamp(const char* root) {
    if (!root || !*root) return 0;
    DIR* dir = opendir(root);
    if (!dir) return 0;

    time_t latest = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        if (strcmp(entry->d_name, "ide_files") == 0) continue;

        char fullPath[PATH_MAX];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", root, entry->d_name);
        struct stat st;
        if (stat(fullPath, &st) != 0) continue;
        if (st.st_mtime > latest) latest = st.st_mtime;
    }
    closedir(dir);
    return latest;
}

void initFileWatcher() {
    head = NULL;
    watchedWorkspacePath[0] = '\0';
    workspaceLastModified = 0;
    nextPollMs = 0;
    pollIntervalMs = file_watcher_poll_interval_ms();
    watcherLogEnabled = -1;
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
    Uint32 now = SDL_GetTicks();
    if (now < nextPollMs) return;
    nextPollMs = now + pollIntervalMs;

    WatchedFile* current = head;
    while (current) {
        struct stat st;
        if (stat(current->file->filePath, &st) == 0) {
            if (st.st_mtime != current->lastModified) {
                if (!current->file->isModified) {
                    if (file_watcher_log_enabled()) {
                        printf("[FileWatcher] Auto-reloading %s\n", current->file->filePath);
                    }
                    reloadOpenFileFromDisk(current->file);
                    current->lastModified = st.st_mtime;
                } else {
                    if (file_watcher_log_enabled()) {
                        printf("[FileWatcher] WARNING: %s changed on disk but has unsaved changes.\n",
                               current->file->filePath);
                    }
                }
            }
        }
        current = current->next;
    }

    if (watchedWorkspacePath[0] != '\0') {
        time_t stamp = compute_workspace_stamp(watchedWorkspacePath);
        if (stamp == 0) return;
        if (workspaceLastModified == 0) {
            workspaceLastModified = stamp;
        } else if (stamp != workspaceLastModified) {
            workspaceLastModified = stamp;
            pendingProjectRefresh = true;
        }
    }
}
