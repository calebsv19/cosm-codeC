#include "tool_project_run_target_helpers.h"

#include "app/GlobalInfo/core_state.h"
#include "app/GlobalInfo/workspace_prefs.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

static bool entry_is_executable_file(const DirEntry* entry) {
    if (!entry || entry->type != ENTRY_FILE || !entry->path) return false;
    {
        struct stat st;
        if (stat(entry->path, &st) != 0) return false;
        return S_ISREG(st.st_mode) && (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH));
    }
}

static DirEntry* find_newest_executable_recursive(DirEntry* entry, time_t* newest_time) {
    if (!entry) return NULL;
    DirEntry* best = NULL;

    if (entry->type == ENTRY_FILE && entry->path) {
        struct stat st;
        if (stat(entry->path, &st) == 0) {
            if (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
                if (st.st_mtime >= *newest_time) {
                    *newest_time = st.st_mtime;
                    best = entry;
                }
            }
        }
    }

    if (entry->type == ENTRY_FOLDER) {
        for (int i = 0; i < entry->childCount; ++i) {
            DirEntry* candidate = find_newest_executable_recursive(entry->children[i], newest_time);
            if (candidate) best = candidate;
        }
    }

    return best;
}

void project_run_target_clear(char* run_target_path, size_t cap) {
    if (!run_target_path || cap == 0) return;
    run_target_path[0] = '\0';
    setRunTargetPath(NULL);
    saveRunTargetPreference(NULL);
}

void project_run_target_update_from_entry(DirEntry* entry, char* run_target_path, size_t cap) {
    if (!run_target_path || cap == 0) return;
    if (!entry) {
        project_run_target_clear(run_target_path, cap);
        return;
    }

    if (entry->type == ENTRY_FILE) {
        if (!entry_is_executable_file(entry)) {
            project_run_target_clear(run_target_path, cap);
            return;
        }
        if (strcmp(run_target_path, entry->path) != 0) {
            snprintf(run_target_path, cap, "%s", entry->path);
            setRunTargetPath(run_target_path);
            saveRunTargetPreference(run_target_path);
        }
    } else {
        time_t newest_time = 0;
        DirEntry* newest = find_newest_executable_recursive(entry, &newest_time);
        if (newest && newest->path) {
            if (strcmp(run_target_path, newest->path) != 0) {
                snprintf(run_target_path, cap, "%s", newest->path);
                setRunTargetPath(run_target_path);
                saveRunTargetPreference(run_target_path);
            }
        } else {
            project_run_target_clear(run_target_path, cap);
        }
    }
}
