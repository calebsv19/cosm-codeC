#include "core/Analysis/analysis_job.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_atomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/Analysis/analysis_cache.h"
#include "core/Analysis/analysis_cache_manifest.h"
#include "core/Analysis/analysis_incremental_policy.h"
#include "core/Analysis/analysis_snapshot.h"
#include "core/Analysis/analysis_status.h"
#include "core/Analysis/analysis_store.h"
#include "core/Analysis/analysis_symbols_store.h"
#include "core/Analysis/analysis_token_store.h"
#include "core/Analysis/analysis_units_store.h"
#include "core/Analysis/include_graph.h"
#include "core/Analysis/include_path_resolver.h"
#include "core/Analysis/library_index.h"
#include "core/Analysis/project_scan.h"
#include "core/LoopResults/completed_results_queue.h"
#include "core/LoopKernel/mainthread_context.h"
#include "core/LoopWake/mainthread_wake.h"

static SDL_Thread* g_thread = NULL;
static bool g_thread_running = false;
static bool g_force_full_refresh = false;
static uint64_t g_analysis_run_id = 0;
static char g_last_error[256];
static char g_project_root[1024];
static char g_build_args[1024];
typedef enum AnalysisJobRunMode {
    ANALYSIS_JOB_RUN_WORKSPACE = 0,
    ANALYSIS_JOB_RUN_LIVE_BUFFER = 1
} AnalysisJobRunMode;
static AnalysisJobRunMode g_run_mode = ANALYSIS_JOB_RUN_WORKSPACE;
#define ANALYSIS_JOB_MAX_FILE_HINTS 32
static char g_file_hints[ANALYSIS_JOB_MAX_FILE_HINTS][1024];
static size_t g_file_hint_count = 0;
static char g_live_file_path[1024];
static char* g_live_contents = NULL;
static size_t g_live_content_length = 0;
static uint64_t g_live_document_revision = 0;
static SDL_atomic_t g_cancel_requested;
static SDL_atomic_t g_slow_mode_next_run;
static SDL_atomic_t g_slow_mode_active;
static SDL_atomic_t g_last_progress_completed;
static SDL_atomic_t g_last_progress_total;

static void analysis_job_clear_live_request(void) {
    free(g_live_contents);
    g_live_contents = NULL;
    g_live_content_length = 0;
    g_live_file_path[0] = '\0';
    g_live_document_revision = 0;
    g_run_mode = ANALYSIS_JOB_RUN_WORKSPACE;
}

bool analysis_job_system_init(void) {
    return true;
}

void analysis_job_system_shutdown(void) {
    // Analysis worker is detached; no shutdown join path.
}

static void analysis_queue_finished_message(bool cancelled, bool had_error) {
    CompletedResult result;
    memset(&result, 0, sizeof(result));
    result.subsystem = COMPLETED_SUBSYSTEM_ANALYSIS;
    result.kind = COMPLETED_RESULT_ANALYSIS_FINISHED;
    result.payload.analysis_finished.analysis_run_id = g_analysis_run_id;
    result.payload.analysis_finished.cancelled = cancelled;
    result.payload.analysis_finished.had_error = had_error;
    result.payload.analysis_finished.library_index_stamp = library_index_combined_stamp();
    snprintf(result.payload.analysis_finished.project_root,
             sizeof(result.payload.analysis_finished.project_root),
             "%s",
             g_project_root);
    result.payload.analysis_finished.project_root[sizeof(result.payload.analysis_finished.project_root) - 1] = '\0';
    completed_results_queue_push(&result);
}

static void analysis_attach_live_document_revision(CompletedResult* result) {
    if (!result || g_run_mode != ANALYSIS_JOB_RUN_LIVE_BUFFER) return;
    if (!g_live_file_path[0] || g_live_document_revision == 0) return;
    result->has_document_revision = true;
    result->document_revision = g_live_document_revision;
    snprintf(result->document_path,
             sizeof(result->document_path),
             "%s",
             g_live_file_path);
    result->document_path[sizeof(result->document_path) - 1] = '\0';
}

static void analysis_queue_symbols_updated_message(uint64_t symbols_stamp) {
    CompletedResult result;
    memset(&result, 0, sizeof(result));
    result.subsystem = COMPLETED_SUBSYSTEM_SYMBOLS;
    result.kind = COMPLETED_RESULT_SYMBOLS_UPDATED;
    result.payload.symbols_updated.analysis_run_id = g_analysis_run_id;
    result.payload.symbols_updated.symbols_stamp = symbols_stamp;
    snprintf(result.payload.symbols_updated.project_root,
             sizeof(result.payload.symbols_updated.project_root),
             "%s",
             g_project_root);
    result.payload.symbols_updated.project_root[sizeof(result.payload.symbols_updated.project_root) - 1] = '\0';
    analysis_attach_live_document_revision(&result);
    completed_results_queue_push(&result);
}

static void analysis_queue_diagnostics_updated_message(uint64_t diagnostics_stamp) {
    CompletedResult result;
    memset(&result, 0, sizeof(result));
    result.subsystem = COMPLETED_SUBSYSTEM_DIAGNOSTICS;
    result.kind = COMPLETED_RESULT_DIAGNOSTICS_UPDATED;
    result.payload.diagnostics_updated.analysis_run_id = g_analysis_run_id;
    result.payload.diagnostics_updated.diagnostics_stamp = diagnostics_stamp;
    snprintf(result.payload.diagnostics_updated.project_root,
             sizeof(result.payload.diagnostics_updated.project_root),
             "%s",
             g_project_root);
    result.payload.diagnostics_updated.project_root[sizeof(result.payload.diagnostics_updated.project_root) - 1] = '\0';
    analysis_attach_live_document_revision(&result);
    completed_results_queue_push(&result);
}

void analysis_job_report_status_update(bool set_status,
                                       int status_value,
                                       bool set_has_cache,
                                       bool has_cache,
                                       bool set_last_error,
                                       const char* last_error) {
    CompletedResult result;
    memset(&result, 0, sizeof(result));
    result.subsystem = COMPLETED_SUBSYSTEM_ANALYSIS;
    result.kind = COMPLETED_RESULT_ANALYSIS_STATUS_UPDATE;
    result.payload.analysis_status_update.analysis_run_id = g_analysis_run_id;
    result.payload.analysis_status_update.set_status = set_status;
    result.payload.analysis_status_update.status_value = status_value;
    result.payload.analysis_status_update.set_has_cache = set_has_cache;
    result.payload.analysis_status_update.has_cache = has_cache;
    result.payload.analysis_status_update.set_last_error = set_last_error;
    if (set_last_error && last_error) {
        snprintf(result.payload.analysis_status_update.last_error,
                 sizeof(result.payload.analysis_status_update.last_error),
                 "%s",
                 last_error);
        result.payload.analysis_status_update.last_error[sizeof(result.payload.analysis_status_update.last_error) - 1] = '\0';
    } else {
        result.payload.analysis_status_update.last_error[0] = '\0';
    }
    snprintf(result.payload.analysis_status_update.project_root,
             sizeof(result.payload.analysis_status_update.project_root),
             "%s",
             g_project_root);
    result.payload.analysis_status_update.project_root[sizeof(result.payload.analysis_status_update.project_root) - 1] = '\0';
    completed_results_queue_push(&result);
}

void analysis_job_report_progress(int completed_files, int total_files) {
    if (completed_files < 0) completed_files = 0;
    if (total_files < 0) total_files = 0;
    if (completed_files > total_files) completed_files = total_files;

    int prev_completed = SDL_AtomicGet(&g_last_progress_completed);
    int prev_total = SDL_AtomicGet(&g_last_progress_total);
    if (prev_completed == completed_files && prev_total == total_files) {
        return;
    }
    SDL_AtomicSet(&g_last_progress_completed, completed_files);
    SDL_AtomicSet(&g_last_progress_total, total_files);

    CompletedResult result;
    memset(&result, 0, sizeof(result));
    result.subsystem = COMPLETED_SUBSYSTEM_ANALYSIS;
    result.kind = COMPLETED_RESULT_ANALYSIS_PROGRESS;
    result.payload.analysis_progress.analysis_run_id = g_analysis_run_id;
    result.payload.analysis_progress.completed_files = completed_files;
    result.payload.analysis_progress.total_files = total_files;
    snprintf(result.payload.analysis_progress.project_root,
             sizeof(result.payload.analysis_progress.project_root),
             "%s",
             g_project_root);
    result.payload.analysis_progress.project_root[sizeof(result.payload.analysis_progress.project_root) - 1] = '\0';
    completed_results_queue_push(&result);
}

static bool library_index_has_entries(void) {
    bool has_entries = false;
    library_index_lock();
    for (size_t b = 0; b < library_index_bucket_count(); ++b) {
        const LibraryBucket* bucket = library_index_get_bucket(b);
        if (bucket && library_index_header_count(bucket) > 0) {
            has_entries = true;
            break;
        }
    }
    library_index_unlock();
    return has_entries;
}

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

static bool path_list_add_file_hints(PathList* list,
                                     const AnalysisSnapshot* cached,
                                     const AnalysisSnapshot* current) {
    if (!list || !current) return false;
    for (size_t i = 0; i < g_file_hint_count; ++i) {
        if (!analysis_incremental_policy_should_analyze_hint(cached, current, g_file_hints[i])) {
            continue;
        }
        if (!path_list_add_unique(list, g_file_hints[i])) {
            return false;
        }
    }
    return true;
}

static bool collect_dependents_for_targets(PathList* targets, PathList* dependents) {
    if (!targets || !dependents) return false;
    size_t seed_count = targets->count;
    for (size_t i = 0; i < seed_count; ++i) {
        char** deps = NULL;
        size_t dep_count = include_graph_collect_dependents(targets->items[i], &deps);
        for (size_t d = 0; d < dep_count; ++d) {
            if (!path_list_add_unique(dependents, deps[d])) {
                include_graph_free_path_list(deps, dep_count);
                return false;
            }
            if (!path_list_add_unique(targets, deps[d])) {
                include_graph_free_path_list(deps, dep_count);
                return false;
            }
        }
        include_graph_free_path_list(deps, dep_count);
    }
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
        analysis_units_store_remove(path);
        include_graph_remove_source(path);
        library_index_remove_source(path);
    }
}

static bool incremental_baseline_invalid_when_clean(const AnalysisSnapshot* current) {
    if (!current) return true;
    // Empty source workspaces are valid: zero targets is expected.
    if (current->file_count == 0) return false;
    if (analysis_store_file_count() == 0) return true;
    if (analysis_symbols_store_file_count() == 0) return true;
    // Tokens are a derived, bounded cache. Large workspaces may intentionally
    // skip token persistence, so token absence alone must not force a full run.
    if (!library_index_has_entries()) return true;
    return false;
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
    if (!diff_ok) {
        analysis_snapshot_free_path_list(dirty_paths, dirty_count);
        analysis_snapshot_free_path_list(removed_paths, removed_count);
        analysis_snapshot_clear(&cached);
        analysis_snapshot_clear(&current);
        return false;
    }
    if (out_stats) {
        out_stats->dirty_count = (int)dirty_count;
        out_stats->removed_count = (int)removed_count;
    }

    // If snapshots match but in-memory stores/index are empty, incremental mode
    // has no baseline to operate on. Force a full rebuild.
    if (dirty_count == 0 && removed_count == 0) {
        if (incremental_baseline_invalid_when_clean(&current)) {
            analysis_snapshot_free_path_list(dirty_paths, dirty_count);
            analysis_snapshot_free_path_list(removed_paths, removed_count);
            analysis_snapshot_clear(&cached);
            analysis_snapshot_clear(&current);
            return false;
        }
    }

    PathList targets;
    PathList dependents;
    path_list_init(&targets);
    path_list_init(&dependents);
    bool paths_ok = true;
    for (size_t i = 0; i < dirty_count; ++i) {
        if (!path_list_add_unique(&targets, dirty_paths[i])) {
            paths_ok = false;
            break;
        }
    }

    if (paths_ok && !path_list_add_file_hints(&targets, &cached, &current)) {
        paths_ok = false;
    }

    if (paths_ok &&
        analysis_incremental_policy_requires_full_for_missing_include_graph(
            include_graph_entry_count(),
            targets.items,
            targets.count,
            removed_paths,
            removed_count,
            NULL,
            0)) {
        paths_ok = false;
    }

    if (paths_ok) {
        paths_ok = collect_dependents_for_targets(&targets, &dependents);
    }

    // Removed files (especially headers) can invalidate dependents.
    if (paths_ok) {
        for (size_t i = 0; i < removed_count; ++i) {
            char** deps = NULL;
            size_t dep_count = include_graph_collect_dependents(removed_paths[i], &deps);
            for (size_t d = 0; d < dep_count; ++d) {
                if (!path_list_add_unique(&dependents, deps[d])) {
                    paths_ok = false;
                    break;
                }
                if (!path_list_add_unique(&targets, deps[d])) {
                    paths_ok = false;
                    break;
                }
            }
            include_graph_free_path_list(deps, dep_count);
            if (!paths_ok) break;
        }
    }

    // Remove stale entries after dependent collection.
    remove_deleted_file_entries(removed_paths, removed_count);

    analysis_snapshot_free_path_list(dirty_paths, dirty_count);
    analysis_snapshot_free_path_list(removed_paths, removed_count);
    analysis_snapshot_clear(&cached);

    if (!paths_ok) {
        path_list_clear(&targets);
        path_list_clear(&dependents);
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
        out_stats->dependent_count = (int)dependents.count;
        out_stats->target_count = (int)targets.count;
    }

    library_index_finalize();
    analysis_store_save(g_project_root);
    analysis_symbols_store_save(g_project_root);
    analysis_token_store_save(g_project_root);
    analysis_units_store_save(g_project_root);
    include_graph_save(g_project_root);
    analysis_snapshot_save(g_project_root, &current);
    path_list_clear(&targets);
    path_list_clear(&dependents);
    analysis_snapshot_clear(&current);
    return true;
}

static int analysis_thread_fn(void* data) {
    (void)data;
    mainthread_context_push_non_owner_scope();
    BuildFlagSet flags = {0};
    SDL_AtomicSet(&g_slow_mode_active, SDL_AtomicGet(&g_slow_mode_next_run));
    SDL_AtomicSet(&g_slow_mode_next_run, 0);
    if (!g_project_root[0]) {
        snprintf(g_last_error, sizeof(g_last_error), "No project root provided");
        analysis_job_report_status_update(true,
                                          ANALYSIS_STATUS_IDLE,
                                          false,
                                          false,
                                          true,
                                          g_last_error);
        analysis_refresh_set_running(false);
        SDL_AtomicSet(&g_slow_mode_active, 0);
        analysis_job_clear_live_request();
        analysis_queue_finished_message(false, true);
        mainthread_wake_push();
        mainthread_context_pop_non_owner_scope();
        return -1;
    }

    gather_build_flags(g_project_root, g_build_args[0] ? g_build_args : NULL, &flags);

    if (analysis_job_cancel_requested()) {
        free_build_flag_set(&flags);
        analysis_job_report_status_update(true,
                                          ANALYSIS_STATUS_IDLE,
                                          false,
                                          false,
                                          true,
                                          NULL);
        analysis_refresh_set_running(false);
        g_thread_running = false;
        g_thread = NULL;
        SDL_AtomicSet(&g_slow_mode_active, 0);
        analysis_job_clear_live_request();
        analysis_queue_finished_message(true, false);
        mainthread_wake_push();
        mainthread_context_pop_non_owner_scope();
        return 0;
    }

    bool force_full = g_force_full_refresh;
    g_force_full_refresh = false;

    if (g_run_mode == ANALYSIS_JOB_RUN_LIVE_BUFFER) {
        analysis_scan_buffer_with_flags(g_project_root,
                                        g_live_file_path,
                                        g_live_contents ? g_live_contents : "",
                                        g_live_content_length,
                                        &flags,
                                        false,
                                        false);
        analysis_status_note_refresh(ANALYSIS_REFRESH_MODE_INCREMENTAL, 0, 0, 0, 1);
    } else {
        IncrementalRunStats run_stats = {0};
        bool incremental_ok = force_full ? false : run_incremental_scan(&flags, &run_stats);
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
    }

    bool cancelled = analysis_job_cancel_requested();
    if (!cancelled) {
        if (g_run_mode != ANALYSIS_JOB_RUN_LIVE_BUFFER) {
            // Persist outputs + metadata
            analysis_cache_save_metadata(g_project_root, g_build_args);
            analysis_cache_save_build_flags(&flags, g_project_root);
            library_index_save(g_project_root);
            analysis_cache_manifest_write_report(g_project_root,
                                                 g_build_args,
                                                 "analysis_persisted");
        }
        analysis_queue_symbols_updated_message(analysis_symbols_store_combined_stamp());
        analysis_queue_diagnostics_updated_message(analysis_store_combined_stamp());
    }

    free_build_flag_set(&flags);
    if (!cancelled) {
        analysis_job_report_status_update(true,
                                          ANALYSIS_STATUS_FRESH,
                                          true,
                                          true,
                                          true,
                                          NULL);
    } else {
        analysis_job_report_status_update(true,
                                          ANALYSIS_STATUS_IDLE,
                                          false,
                                          false,
                                          true,
                                          NULL);
    }
    analysis_queue_finished_message(cancelled, false);
    analysis_refresh_set_running(false);
    g_thread_running = false;
    g_thread = NULL;
    analysis_job_clear_live_request();
    SDL_AtomicSet(&g_slow_mode_active, 0);
    mainthread_wake_push();
    mainthread_context_pop_non_owner_scope();
    return 0;
}

void analysis_force_full_refresh_next_run(void) {
    g_force_full_refresh = true;
}

void analysis_job_set_slow_mode_next_run(bool enabled) {
    SDL_AtomicSet(&g_slow_mode_next_run, enabled ? 1 : 0);
}

void analysis_job_maybe_throttle(void) {
    if (SDL_AtomicGet(&g_slow_mode_active)) {
        SDL_Delay(2);
    }
}

void analysis_job_request_cancel(void) {
    SDL_AtomicSet(&g_cancel_requested, 1);
}

bool analysis_job_cancel_requested(void) {
    return SDL_AtomicGet(&g_cancel_requested) != 0;
}

void start_async_workspace_analysis(const char* project_root, const char* build_args, uint64_t run_id) {
    start_async_workspace_analysis_with_file_hints(project_root, build_args, run_id, NULL, 0);
}

void start_async_workspace_analysis_with_file_hints(const char* project_root,
                                                    const char* build_args,
                                                    uint64_t run_id,
                                                    const char* const* file_hints,
                                                    size_t file_hint_count) {
    if (g_thread_running) return; // already running
    if (!project_root || !*project_root) return;
    SDL_AtomicSet(&g_cancel_requested, 0);
    SDL_AtomicSet(&g_last_progress_completed, -1);
    SDL_AtomicSet(&g_last_progress_total, -1);
    memset(g_last_error, 0, sizeof(g_last_error));
    g_run_mode = ANALYSIS_JOB_RUN_WORKSPACE;
    analysis_job_clear_live_request();
    g_analysis_run_id = run_id;
    snprintf(g_project_root, sizeof(g_project_root), "%s", project_root);
    g_project_root[sizeof(g_project_root) - 1] = '\0';
    if (build_args) {
        snprintf(g_build_args, sizeof(g_build_args), "%s", build_args);
        g_build_args[sizeof(g_build_args) - 1] = '\0';
    } else {
        g_build_args[0] = '\0';
    }
    g_file_hint_count = 0;
    if (file_hints) {
        for (size_t i = 0; i < file_hint_count && g_file_hint_count < ANALYSIS_JOB_MAX_FILE_HINTS; ++i) {
            const char* hint = file_hints[i];
            if (!hint || !hint[0]) continue;
            snprintf(g_file_hints[g_file_hint_count],
                     sizeof(g_file_hints[g_file_hint_count]),
                     "%s",
                     hint);
            g_file_hints[g_file_hint_count][sizeof(g_file_hints[g_file_hint_count]) - 1] = '\0';
            g_file_hint_count++;
        }
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
        analysis_job_report_status_update(false,
                                          0,
                                          false,
                                          false,
                                          true,
                                          g_last_error);
        analysis_refresh_set_running(false);
        analysis_job_report_status_update(true,
                                          ANALYSIS_STATUS_IDLE,
                                          false,
                                          false,
                                          false,
                                          NULL);
        g_thread_running = false;
        mainthread_wake_push();
        return;
    }
    SDL_DetachThread(g_thread);
    // Thread pointer is no longer joinable; keep it null to allow re-entry when finished.
    g_thread = NULL;
}

void start_async_live_buffer_analysis(const char* project_root,
                                      const char* build_args,
                                      uint64_t run_id,
                                      const char* file_path,
                                      const char* contents,
                                      size_t content_length,
                                      uint64_t document_revision) {
    if (g_thread_running) return;
    if (!project_root || !*project_root || !file_path || !*file_path || !contents) return;
    char* owned_contents = (char*)malloc(content_length + 1u);
    if (!owned_contents) return;
    memcpy(owned_contents, contents, content_length);
    owned_contents[content_length] = '\0';

    SDL_AtomicSet(&g_cancel_requested, 0);
    SDL_AtomicSet(&g_last_progress_completed, -1);
    SDL_AtomicSet(&g_last_progress_total, -1);
    memset(g_last_error, 0, sizeof(g_last_error));
    g_run_mode = ANALYSIS_JOB_RUN_LIVE_BUFFER;
    g_analysis_run_id = run_id;
    snprintf(g_project_root, sizeof(g_project_root), "%s", project_root);
    g_project_root[sizeof(g_project_root) - 1] = '\0';
    if (build_args) {
        snprintf(g_build_args, sizeof(g_build_args), "%s", build_args);
        g_build_args[sizeof(g_build_args) - 1] = '\0';
    } else {
        g_build_args[0] = '\0';
    }
    g_file_hint_count = 0;
    snprintf(g_live_file_path, sizeof(g_live_file_path), "%s", file_path);
    g_live_file_path[sizeof(g_live_file_path) - 1] = '\0';
    free(g_live_contents);
    g_live_contents = owned_contents;
    g_live_content_length = content_length;
    g_live_document_revision = document_revision;

    analysis_status_set(ANALYSIS_STATUS_REFRESHING);
    analysis_refresh_set_running(true);
    g_thread_running = true;
    g_thread = SDL_CreateThreadWithStackSize(analysis_thread_fn,
                                             "analysis_live_buffer",
                                             8 * 1024 * 1024,
                                             NULL);
    if (!g_thread) {
        snprintf(g_last_error, sizeof(g_last_error), "Failed to create analysis thread: %s", SDL_GetError());
        analysis_job_report_status_update(false,
                                          0,
                                          false,
                                          false,
                                          true,
                                          g_last_error);
        analysis_refresh_set_running(false);
        analysis_job_report_status_update(true,
                                          ANALYSIS_STATUS_IDLE,
                                          false,
                                          false,
                                          false,
                                          NULL);
        g_thread_running = false;
        analysis_job_clear_live_request();
        mainthread_wake_push();
        return;
    }
    SDL_DetachThread(g_thread);
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
