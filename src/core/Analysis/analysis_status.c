#include "core/Analysis/analysis_status.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>

#include "core/Analysis/analysis_scheduler.h"

static AnalysisStatus g_status = ANALYSIS_STATUS_IDLE;
static bool g_refresh_pending = false;
static bool g_refresh_running = false;
static bool g_has_cache = false;
static AnalysisRefreshMode g_refresh_mode = ANALYSIS_REFRESH_MODE_NONE;
static int g_dirty_files = 0;
static int g_removed_files = 0;
static int g_dependent_files = 0;
static int g_target_files = 0;
static char g_last_error[256] = {0};
static bool g_frontend_logs_enabled = false;
static SDL_mutex* g_status_mutex = NULL;

static void status_lock(void) {
    if (g_status_mutex) SDL_LockMutex(g_status_mutex);
}

static void status_unlock(void) {
    if (g_status_mutex) SDL_UnlockMutex(g_status_mutex);
}

void analysis_status_init(void) {
    if (!g_status_mutex) {
        g_status_mutex = SDL_CreateMutex();
    }
    status_lock();
    g_status = ANALYSIS_STATUS_IDLE;
    g_refresh_pending = false;
    g_refresh_running = false;
    g_has_cache = false;
    g_refresh_mode = ANALYSIS_REFRESH_MODE_NONE;
    g_dirty_files = 0;
    g_removed_files = 0;
    g_dependent_files = 0;
    g_target_files = 0;
    g_last_error[0] = '\0';
    const char* env = getenv("IDE_ANALYSIS_FRONTEND_LOGS");
    g_frontend_logs_enabled = (env && env[0] == '1');
    status_unlock();
}

AnalysisStatus analysis_status_get(void) {
    status_lock();
    AnalysisStatus out = g_status;
    status_unlock();
    return out;
}

void analysis_status_set(AnalysisStatus s) {
    status_lock();
    g_status = s;
    status_unlock();
}

void analysis_request_refresh(void) {
    status_lock();
    g_refresh_pending = true;
    status_unlock();
    analysis_scheduler_request(ANALYSIS_REASON_MANUAL_REFRESH, false);
}

bool analysis_refresh_pending(void) {
    status_lock();
    bool out = g_refresh_pending;
    status_unlock();
    return out;
}

bool analysis_refresh_running(void) {
    status_lock();
    bool out = g_refresh_running;
    status_unlock();
    return out;
}

void analysis_refresh_set_running(bool running) {
    status_lock();
    g_refresh_running = running;
    if (!running) {
        g_refresh_pending = false;
    }
    status_unlock();
}

void analysis_status_set_has_cache(bool has) {
    status_lock();
    g_has_cache = has;
    status_unlock();
}

void analysis_status_set_last_error(const char* msg) {
    status_lock();
    if (!msg) {
        g_last_error[0] = '\0';
        status_unlock();
        return;
    }
    snprintf(g_last_error, sizeof(g_last_error), "%s", msg);
    g_last_error[sizeof(g_last_error) - 1] = '\0';
    status_unlock();
}

void analysis_status_note_refresh(AnalysisRefreshMode mode,
                                  int dirty_files,
                                  int removed_files,
                                  int dependent_files,
                                  int target_files) {
    status_lock();
    g_refresh_mode = mode;
    g_dirty_files = (dirty_files < 0) ? 0 : dirty_files;
    g_removed_files = (removed_files < 0) ? 0 : removed_files;
    g_dependent_files = (dependent_files < 0) ? 0 : dependent_files;
    g_target_files = (target_files < 0) ? 0 : target_files;
    status_unlock();
}

void analysis_status_snapshot(AnalysisStatusSnapshot* out) {
    if (!out) return;
    status_lock();
    out->status = g_status;
    out->updating = (g_status == ANALYSIS_STATUS_REFRESHING || g_refresh_running);
    out->has_cache = g_has_cache;
    out->refresh_mode = g_refresh_mode;
    out->dirty_files = g_dirty_files;
    out->removed_files = g_removed_files;
    out->dependent_files = g_dependent_files;
    out->target_files = g_target_files;
    snprintf(out->last_error, sizeof(out->last_error), "%s", g_last_error);
    out->last_error[sizeof(out->last_error) - 1] = '\0';
    status_unlock();
}

bool analysis_frontend_logs_enabled(void) {
    status_lock();
    bool out = g_frontend_logs_enabled;
    status_unlock();
    return out;
}

void analysis_set_frontend_logs_enabled(bool enabled) {
    status_lock();
    g_frontend_logs_enabled = enabled;
    status_unlock();
}

void analysis_toggle_frontend_logs_enabled(void) {
    status_lock();
    g_frontend_logs_enabled = !g_frontend_logs_enabled;
    status_unlock();
}
