#include "core/Analysis/analysis_job.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>

#include "core/Analysis/analysis_cache.h"
#include "core/Analysis/analysis_status.h"
#include "core/Analysis/include_path_resolver.h"
#include "core/Analysis/library_index.h"
#include "core/Analysis/project_scan.h"

static SDL_Thread* g_thread = NULL;
static bool g_thread_running = false;
static char g_last_error[256];
static char g_project_root[1024];
static char g_build_args[1024];

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

    // Run scans using the same flags
    analysis_scan_workspace_with_flags(g_project_root, &flags, false /*update_engine*/);
    library_index_build_workspace_with_flags(g_project_root, &flags);

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
