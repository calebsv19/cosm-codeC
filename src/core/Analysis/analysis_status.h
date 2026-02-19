#ifndef ANALYSIS_STATUS_H
#define ANALYSIS_STATUS_H

#include <stdbool.h>

typedef enum {
    ANALYSIS_STATUS_IDLE = 0,
    ANALYSIS_STATUS_STALE_LOADING,
    ANALYSIS_STATUS_REFRESHING,
    ANALYSIS_STATUS_FRESH
} AnalysisStatus;

typedef struct {
    AnalysisStatus status;
    bool updating;
    bool has_cache;
    char last_error[256];
} AnalysisStatusSnapshot;

void analysis_status_init(void);
AnalysisStatus analysis_status_get(void);
void analysis_status_set(AnalysisStatus s);

// Refresh lifecycle flags
void analysis_request_refresh(void);
bool analysis_refresh_pending(void);
bool analysis_refresh_running(void);
void analysis_refresh_set_running(bool running);

// Cache + error tracking
void analysis_status_set_has_cache(bool has);
void analysis_status_set_last_error(const char* msg);
void analysis_status_snapshot(AnalysisStatusSnapshot* out);

#endif // ANALYSIS_STATUS_H
