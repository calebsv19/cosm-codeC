#ifndef ANALYSIS_STATUS_H
#define ANALYSIS_STATUS_H

#include <stdbool.h>

typedef enum {
    ANALYSIS_STATUS_IDLE = 0,
    ANALYSIS_STATUS_STALE_LOADING,
    ANALYSIS_STATUS_REFRESHING,
    ANALYSIS_STATUS_FRESH
} AnalysisStatus;

typedef enum {
    ANALYSIS_REFRESH_MODE_NONE = 0,
    ANALYSIS_REFRESH_MODE_INCREMENTAL,
    ANALYSIS_REFRESH_MODE_FULL
} AnalysisRefreshMode;

typedef struct {
    AnalysisStatus status;
    bool updating;
    bool has_cache;
    AnalysisRefreshMode refresh_mode;
    int dirty_files;
    int removed_files;
    int dependent_files;
    int target_files;
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
void analysis_status_note_refresh(AnalysisRefreshMode mode,
                                  int dirty_files,
                                  int removed_files,
                                  int dependent_files,
                                  int target_files);
void analysis_status_set_progress(int completed_files, int total_files);
void analysis_status_get_progress(int* out_completed_files, int* out_total_files);
void analysis_status_snapshot(AnalysisStatusSnapshot* out);

// Frontend log verbosity control used by analysis scans.
bool analysis_frontend_logs_enabled(void);
void analysis_set_frontend_logs_enabled(bool enabled);
void analysis_toggle_frontend_logs_enabled(void);

#endif // ANALYSIS_STATUS_H
