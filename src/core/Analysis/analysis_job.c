#include "core/Analysis/analysis_job.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/Analysis/analysis_cache.h"
#include "core/Analysis/analysis_snapshot.h"
#include "core/Analysis/analysis_status.h"
#include "core/Analysis/analysis_store.h"
#include "core/Analysis/analysis_symbols_store.h"
#include "core/Analysis/analysis_token_store.h"
#include "core/Analysis/include_graph.h"
#include "core/Analysis/include_path_resolver.h"
#include "core/Analysis/library_index.h"
#include "core/Analysis/project_scan.h"

static SDL_Thread* g_thread = NULL;
static bool g_thread_running = false;
static char g_last_error[256];
static char g_project_root[1024];
static char g_build_args[1024];

typedef struct {
    char** items;
    size_t count;
    size_t cap;
} PathList;

typedef struct {
    int dirty_count;
    int removed_count;
    int dependent_count;
    int target_count;
} IncrementalRunStats;

static void path_list_init(PathList* list) {
    if (!list) return;
    list->items = NULL;
    list->count = 0;
    list->cap = 0;
}

static void path_list_clear(PathList* list) {
    if (!list) return;
    for (size_t i = 0; i < list->count; ++i) {
        free(list->items[i]);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->cap = 0;
}

static bool path_list_contains(const PathList* list, const char* path) {
    if (!list || !path || !*path) return false;
    for (size_t i = 0; i < list->count; ++i) {
        if (list->items[i] && strcmp(list->items[i], path) == 0) {
            return true;
        }
    }
    return false;
}

static bool path_list_add_unique(PathList* list, const char* path) {
    if (!list || !path || !*path) return false;
    if (path_list_contains(list, path)) return true;

    if (list->count >= list->cap) {
        size_t new_cap = list->cap ? list->cap * 2 : 32;
        char** grown = (char**)realloc(list->items, new_cap * sizeof(char*));
        if (!grown) return false;
        list->items = grown;
        list->cap = new_cap;
    }

    list->items[list->count] = strdup(path);
    if (!list->items[list->count]) return false;
    list->count++;
    return true;
}

static void remove_deleted_file_entries(char** removed_paths, size_t removed_count) {
    if (!removed_paths || removed_count == 0) return;
    for (size_t i = 0; i < removed_count; ++i) {
        const char* path = removed_paths[i];
        if (!path || !*path) continue;
        analysis_store_remove(path);
        analysis_symbols_store_remove(path);
        analysis_token_store_remove(path);
        include_graph_remove_source(path);
        library_index_remove_source(path);
    }
}

static bool run_incremental_scan(const BuildFlagSet* flags, IncrementalRunStats* out_stats) {
    if (!flags) return false;
    if (out_stats) {
        out_stats->dirty_count = 0;
        out_stats->removed_count = 0;
        out_stats->dependent_count = 0;
        out_stats->target_count = 0;
    }

    AnalysisSnapshot cached = {0};
    AnalysisSnapshot current = {0};
    analysis_snapshot_init(&cached);
    analysis_snapshot_init(&current);

    bool loaded_cached = analysis_snapshot_load(g_project_root, &cached);
    bool scanned_current = analysis_snapshot_scan_workspace(g_project_root, &current);
    if (!loaded_cached || !scanned_current) {
        analysis_snapshot_clear(&cached);
        analysis_snapshot_clear(&current);
        return false;
    }

    char** dirty_paths = NULL;
    size_t dirty_count = 0;
    char** removed_paths = NULL;
    size_t removed_count = 0;
    bool diff_ok = analysis_snapshot_compute_dirty_sets(&cached, &current,
                                                        &dirty_paths, &dirty_count,
                                                        &removed_paths, &removed_count);
    analysis_snapshot_clear(&cached);
    if (!diff_ok) {
        analysis_snapshot_free_path_list(dirty_paths, dirty_count);
        analysis_snapshot_free_path_list(removed_paths, removed_count);
        analysis_snapshot_clear(&current);
        return false;
    }
    if (out_stats) {
        out_stats->dirty_count = (int)dirty_count;
        out_stats->removed_count = (int)removed_count;
    }

    PathList targets;
    path_list_init(&targets);
    bool paths_ok = true;
    for (size_t i = 0; i < dirty_count; ++i) {
        if (!path_list_add_unique(&targets, dirty_paths[i])) {
            paths_ok = false;
            break;
        }
    }

    if (paths_ok) {
        for (size_t i = 0; i < dirty_count; ++i) {
            char** deps = NULL;
            size_t dep_count = include_graph_collect_dependents(dirty_paths[i], &deps);
            for (size_t d = 0; d < dep_count; ++d) {
                if (!path_list_add_unique(&targets, deps[d])) {
                    paths_ok = false;
                    break;
                }
            }
            if (out_stats) out_stats->dependent_count += (int)dep_count;
            include_graph_free_path_list(deps, dep_count);
            if (!paths_ok) break;
        }
    }

    // Removed files (especially headers) can invalidate dependents.
    if (paths_ok) {
        for (size_t i = 0; i < removed_count; ++i) {
            char** deps = NULL;
            size_t dep_count = include_graph_collect_dependents(removed_paths[i], &deps);
            for (size_t d = 0; d < dep_count; ++d) {
                if (!path_list_add_unique(&targets, deps[d])) {
                    paths_ok = false;
                    break;
                }
            }
            if (out_stats) out_stats->dependent_count += (int)dep_count;
            include_graph_free_path_list(deps, dep_count);
            if (!paths_ok) break;
        }
    }

    // Remove stale entries after dependent collection.
    remove_deleted_file_entries(removed_paths, removed_count);

    analysis_snapshot_free_path_list(dirty_paths, dirty_count);
    analysis_snapshot_free_path_list(removed_paths, removed_count);

    if (!paths_ok) {
        path_list_clear(&targets);
        analysis_snapshot_clear(&current);
        return false;
    }

    if (targets.count > 0) {
        analysis_scan_files_with_flags(g_project_root,
                                       (const char* const*)targets.items,
                                       targets.count,
                                       flags,
                                       false,
                                       false);
    }
    if (out_stats) {
        out_stats->target_count = (int)targets.count;
    }

    library_index_finalize();
    analysis_store_save(g_project_root);
    analysis_symbols_store_save(g_project_root);
    analysis_token_store_save(g_project_root);
    include_graph_save(g_project_root);
    analysis_snapshot_save(g_project_root, &current);
    path_list_clear(&targets);
    analysis_snapshot_clear(&current);
    return true;
}

static int analysis_thread_fn(void* data) {
    (void)data;
    BuildFlagSet flags = {0};
    if (!g_project_root[0]) {
        snprintf(g_last_error, sizeof(g_last_error), "No project root provided");
        analysis_status_set(ANALYSIS_STATUS_IDLE);
        analysis_refresh_set_running(false);
        return -1;
    }

    gather_build_flags(g_project_root, g_build_args[0] ? g_build_args : NULL, &flags);

    IncrementalRunStats run_stats = {0};
    bool incremental_ok = run_incremental_scan(&flags, &run_stats);
    if (!incremental_ok) {
        // Fallback path for first run, invalid snapshots, or incremental errors.
        analysis_scan_workspace_with_flags(g_project_root, &flags, false /*update_engine*/);
        library_index_build_workspace_with_flags(g_project_root, &flags);
        analysis_snapshot_refresh_and_save(g_project_root);
        analysis_status_note_refresh(ANALYSIS_REFRESH_MODE_FULL, 0, 0, 0, 0);
    } else {
        analysis_status_note_refresh(ANALYSIS_REFRESH_MODE_INCREMENTAL,
                                     run_stats.dirty_count,
                                     run_stats.removed_count,
                                     run_stats.dependent_count,
                                     run_stats.target_count);
    }

    // Persist outputs + metadata
    analysis_cache_save_metadata(g_project_root, g_build_args);
    analysis_cache_save_build_flags(&flags, g_project_root);
    library_index_save(g_project_root);

    free_build_flag_set(&flags);
    analysis_status_set_has_cache(true);
    analysis_status_set(ANALYSIS_STATUS_FRESH);
    analysis_status_set_last_error(NULL);
    analysis_refresh_set_running(false);
    g_thread_running = false;
    g_thread = NULL;
    return 0;
}

void start_async_workspace_analysis(const char* project_root, const char* build_args) {
    if (g_thread_running) return; // already running
    if (!project_root || !*project_root) return;
    memset(g_last_error, 0, sizeof(g_last_error));
    snprintf(g_project_root, sizeof(g_project_root), "%s", project_root);
    g_project_root[sizeof(g_project_root) - 1] = '\0';
    if (build_args) {
        snprintf(g_build_args, sizeof(g_build_args), "%s", build_args);
        g_build_args[sizeof(g_build_args) - 1] = '\0';
    } else {
        g_build_args[0] = '\0';
    }

    analysis_status_set(ANALYSIS_STATUS_REFRESHING);
    analysis_refresh_set_running(true);
    g_thread_running = true;
    // Semantic analysis can recurse deeply on large projects; use a larger stack.
    g_thread = SDL_CreateThreadWithStackSize(analysis_thread_fn,
                                             "analysis_worker",
                                             8 * 1024 * 1024,
                                             NULL);
    if (!g_thread) {
        snprintf(g_last_error, sizeof(g_last_error), "Failed to create analysis thread: %s", SDL_GetError());
        analysis_status_set_last_error(g_last_error);
        analysis_refresh_set_running(false);
        analysis_status_set(ANALYSIS_STATUS_IDLE);
        g_thread_running = false;
        return;
    }
    SDL_DetachThread(g_thread);
    // Thread pointer is no longer joinable; keep it null to allow re-entry when finished.
    g_thread = NULL;
}

bool analysis_job_is_running(void) {
    return g_thread_running;
}

const char* analysis_job_last_error(void) {
    return g_last_error;
}

// Polling helper; join thread when finished
void analysis_job_poll(void) {
    // No-op; thread is detached and signals completion via status flags.
}
