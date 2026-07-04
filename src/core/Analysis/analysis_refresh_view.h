#ifndef ANALYSIS_REFRESH_VIEW_H
#define ANALYSIS_REFRESH_VIEW_H

#include <stddef.h>

#include "core/Analysis/analysis_scheduler.h"
#include "core/Analysis/analysis_status.h"

typedef struct {
    AnalysisStatusSnapshot status;
    AnalysisSchedulerSnapshot scheduler;
    int progress_completed;
    int progress_total;
} AnalysisRefreshViewSnapshot;

void analysis_refresh_view_capture(AnalysisRefreshViewSnapshot* out);
void analysis_refresh_view_format_status_text(const AnalysisRefreshViewSnapshot* view,
                                              char* out,
                                              size_t out_cap);

#endif // ANALYSIS_REFRESH_VIEW_H
