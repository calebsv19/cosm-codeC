#include "core/Analysis/analysis_status.h"

#include <stdio.h>

static AnalysisStatus g_status = ANALYSIS_STATUS_IDLE;
static bool g_refresh_pending = false;
static bool g_refresh_running = false;
static bool g_has_cache = false;
static char g_last_error[256] = {0};

void analysis_status_init(void) {
    g_status = ANALYSIS_STATUS_IDLE;
    g_refresh_pending = false;
    g_refresh_running = false;
    g_has_cache = false;
    g_last_error[0] = '\0';
}

AnalysisStatus analysis_status_get(void) {
    return g_status;
}

void analysis_status_set(AnalysisStatus s) {
    g_status = s;
}

void analysis_request_refresh(void) {
    g_refresh_pending = true;
}

bool analysis_refresh_pending(void) {
    return g_refresh_pending;
}

bool analysis_refresh_running(void) {
    return g_refresh_running;
}

void analysis_refresh_set_running(bool running) {
    g_refresh_running = running;
    if (!running) {
        g_refresh_pending = false;
    }
}

void analysis_status_set_has_cache(bool has) {
    g_has_cache = has;
}

void analysis_status_set_last_error(const char* msg) {
    if (!msg) {
        g_last_error[0] = '\0';
        return;
    }
    snprintf(g_last_error, sizeof(g_last_error), "%s", msg);
    g_last_error[sizeof(g_last_error) - 1] = '\0';
}

void analysis_status_snapshot(AnalysisStatusSnapshot* out) {
    if (!out) return;
    out->status = g_status;
    out->updating = (g_status == ANALYSIS_STATUS_REFRESHING || g_refresh_running);
    out->has_cache = g_has_cache;
    snprintf(out->last_error, sizeof(out->last_error), "%s", g_last_error);
    out->last_error[sizeof(out->last_error) - 1] = '\0';
}
