#ifndef ANALYSIS_SCHEDULER_H
#define ANALYSIS_SCHEDULER_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    ANALYSIS_REASON_NONE = 0,
    ANALYSIS_REASON_STARTUP = 1u << 0,
    ANALYSIS_REASON_WORKSPACE_RELOAD = 1u << 1,
    ANALYSIS_REASON_WATCHER_CHANGE = 1u << 2,
    ANALYSIS_REASON_MANUAL_REFRESH = 1u << 3,
    ANALYSIS_REASON_PROJECT_MUTATION = 1u << 4,
    ANALYSIS_REASON_LIBRARY_PANEL_REFRESH = 1u << 5
} AnalysisRefreshReason;

typedef struct {
    uint64_t active_run_id;
    uint64_t last_completed_run_id;
    bool running;
    bool pending;
    bool pending_force_full;
    unsigned int pending_reason_mask;
    unsigned int active_reason_mask;
} AnalysisSchedulerSnapshot;

void analysis_scheduler_init(void);
void analysis_scheduler_request(AnalysisRefreshReason reason, bool force_full);
void analysis_scheduler_tick(const char* project_root, const char* build_args);
bool analysis_scheduler_running(void);
void analysis_scheduler_snapshot(AnalysisSchedulerSnapshot* out);
const char* analysis_scheduler_reason_mask_to_string(unsigned int reason_mask,
                                                     char* buf,
                                                     int buf_size);

#endif // ANALYSIS_SCHEDULER_H
