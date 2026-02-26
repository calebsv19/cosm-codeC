
#include "file_watcher.h"
#include "app/GlobalInfo/project.h"
#include "core/Analysis/analysis_scheduler.h"
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
static Uint32 pollIntervalMs = 250;
static Uint32 suppressWorkspaceRefreshUntilMs = 0;
static Uint32 suppressInternalRefreshUntilMs = 0;
static Uint32 workspaceDebounceMs = 350;
static Uint32 workspaceCooldownMs = 1200;
static Uint32 lastWorkspaceTriggerMs = 0;
static time_t pendingWorkspaceStamp = 0;
static Uint32 pendingWorkspaceStampSinceMs = 0;
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

static Uint32 file_watcher_debounce_ms(void) {
    const char* env = getenv("IDE_WATCHER_DEBOUNCE_MS");
    if (!env || !env[0]) return workspaceDebounceMs;
    char* end = NULL;
    long v = strtol(env, &end, 10);
    if (end == env || v < 50 || v > 5000) return workspaceDebounceMs;
    return (Uint32)v;
}

static Uint32 file_watcher_cooldown_ms(void) {
    const char* env = getenv("IDE_WATCHER_COOLDOWN_MS");
    if (!env || !env[0]) return workspaceCooldownMs;
    char* end = NULL;
    long v = strtol(env, &end, 10);
    if (end == env || v < 100 || v > 10000) return workspaceCooldownMs;
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
    pollIntervalMs = file_watcher_poll_interval_ms();
    workspaceDebounceMs = file_watcher_debounce_ms();
    workspaceCooldownMs = file_watcher_cooldown_ms();
    suppressWorkspaceRefreshUntilMs = 0;
    suppressInternalRefreshUntilMs = 0;
    lastWorkspaceTriggerMs = 0;
    pendingWorkspaceStamp = 0;
    pendingWorkspaceStampSinceMs = 0;
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
    pendingWorkspaceStamp = 0;
    pendingWorkspaceStampSinceMs = 0;
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

    size_t len = strlen(path);
    while (len > 1 && (path[len - 1] == '/' || path[len - 1] == '\\')) {
        len--;
    }
    if (len >= sizeof(watchedWorkspacePath)) {
        len = sizeof(watchedWorkspacePath) - 1;
    }
    memcpy(watchedWorkspacePath, path, len);
    watchedWorkspacePath[len] = '\0';
    workspaceLastModified = 0;
    pendingWorkspaceStamp = 0;
    pendingWorkspaceStampSinceMs = 0;
    lastWorkspaceTriggerMs = 0;
}

void suppressWorkspaceWatchRefreshForMs(unsigned int durationMs) {
    Uint32 now = SDL_GetTicks();
    suppressWorkspaceRefreshUntilMs = now + (Uint32)durationMs;
}

void suppressInternalWatcherRefreshForMs(unsigned int durationMs) {
    Uint32 now = SDL_GetTicks();
    suppressInternalRefreshUntilMs = now + (Uint32)durationMs;
}

void pollFileWatcher() {
    Uint32 now = SDL_GetTicks();

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
        if (now < suppressWorkspaceRefreshUntilMs) {
            workspaceLastModified = stamp;
            pendingWorkspaceStamp = 0;
            pendingWorkspaceStampSinceMs = 0;
            if (file_watcher_log_enabled()) {
                printf("[FileWatcher] suppress workspace-switch active\n");
            }
            return;
        }
        if (now < suppressInternalRefreshUntilMs) {
            workspaceLastModified = stamp;
            pendingWorkspaceStamp = 0;
            pendingWorkspaceStampSinceMs = 0;
            if (file_watcher_log_enabled()) {
                printf("[FileWatcher] suppress internal-write active\n");
            }
            return;
        }
        if (workspaceLastModified == 0) {
            workspaceLastModified = stamp;
            return;
        }
        if (stamp == workspaceLastModified) {
            pendingWorkspaceStamp = 0;
            pendingWorkspaceStampSinceMs = 0;
            return;
        }

        if (pendingWorkspaceStamp != stamp) {
            pendingWorkspaceStamp = stamp;
            pendingWorkspaceStampSinceMs = now;
            if (file_watcher_log_enabled()) {
                printf("[FileWatcher] stamp changed, debounce started (stamp=%lld)\n",
                       (long long)stamp);
            }
            return;
        }

        if ((now - pendingWorkspaceStampSinceMs) < workspaceDebounceMs) {
            if (file_watcher_log_enabled()) {
                printf("[FileWatcher] debounce waiting (%u/%u ms)\n",
                       (unsigned int)(now - pendingWorkspaceStampSinceMs),
                       (unsigned int)workspaceDebounceMs);
            }
            return;
        }

        if ((now - lastWorkspaceTriggerMs) < workspaceCooldownMs) {
            if (file_watcher_log_enabled()) {
                printf("[FileWatcher] cooldown suppress (%u/%u ms)\n",
                       (unsigned int)(now - lastWorkspaceTriggerMs),
                       (unsigned int)workspaceCooldownMs);
            }
            return;
        }

        if (stamp != workspaceLastModified) {
            workspaceLastModified = stamp;
            pendingWorkspaceStamp = 0;
            pendingWorkspaceStampSinceMs = 0;
            lastWorkspaceTriggerMs = now;
            if (file_watcher_log_enabled()) {
                printf("[FileWatcher] trigger refresh (stamp=%lld)\n", (long long)stamp);
            }
            queueProjectRefresh(ANALYSIS_REASON_WATCHER_CHANGE);
        }
    }
}

Uint32 fileWatcherPollIntervalMs(void) {
    Uint32 v = file_watcher_poll_interval_ms();
    if (v < 50) v = 50;
    return v;
}
