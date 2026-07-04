#include "core/Analysis/analysis_refresh_view.h"

#include <stdio.h>
#include <string.h>

static const char* analysis_refresh_view_reason_prefix(const AnalysisStartupAudit* audit) {
    if (!audit || !audit->valid || !audit->refresh_reason[0]) return NULL;
    switch (audit->refresh_intent) {
        case ANALYSIS_STARTUP_REFRESH_FULL_REQUIRED:
            return "Analysis refresh needed";
        case ANALYSIS_STARTUP_REFRESH_INCREMENTAL_TARGETS:
            return "Analysis refresh queued";
        case ANALYSIS_STARTUP_REFRESH_INCREMENTAL_VERIFY_NOOP:
            return "Analysis startup verify";
        case ANALYSIS_STARTUP_REFRESH_UNKNOWN:
        default:
            return "Analysis refresh";
    }
}

void analysis_refresh_view_capture(AnalysisRefreshViewSnapshot* out) {
    if (!out) return;
    memset(out, 0, sizeof(*out));
    analysis_status_snapshot(&out->status);
    analysis_status_get_progress(&out->progress_completed, &out->progress_total);
    analysis_scheduler_snapshot(&out->scheduler);
}

void analysis_refresh_view_format_status_text(const AnalysisRefreshViewSnapshot* view,
                                              char* out,
                                              size_t out_cap) {
    if (!view || !out || out_cap == 0) return;
    out[0] = '\0';

    const AnalysisStatusSnapshot* status = &view->status;
    const AnalysisSchedulerSnapshot* scheduler = &view->scheduler;
    const char* queued_save = scheduler->pending_editor_save_diagnostics
        ? " + save queued"
        : "";

    if (status->updating) {
        if (view->progress_total > 0) {
            if (scheduler->active_run_id) {
                snprintf(out,
                         out_cap,
                         "Updating %d/%d (#%llu)%s",
                         view->progress_completed,
                         view->progress_total,
                         (unsigned long long)scheduler->active_run_id,
                         queued_save);
            } else {
                snprintf(out,
                         out_cap,
                         "Updating %d/%d%s",
                         view->progress_completed,
                         view->progress_total,
                         queued_save);
            }
        } else if (scheduler->active_run_id) {
            snprintf(out,
                     out_cap,
                     "Updating (#%llu)%s",
                     (unsigned long long)scheduler->active_run_id,
                     queued_save);
        } else {
            snprintf(out, out_cap, "Updating%s", queued_save);
        }
    } else if (scheduler->pending_editor_save_diagnostics) {
        snprintf(out, out_cap, "Save diagnostics queued");
    } else if (status->last_error[0]) {
        snprintf(out, out_cap, "Analysis error: %s", status->last_error);
    } else if (status->status == ANALYSIS_STATUS_STALE_LOADING &&
               status->has_startup_audit &&
               status->startup_audit.refresh_reason[0]) {
        const char* prefix = analysis_refresh_view_reason_prefix(&status->startup_audit);
        snprintf(out,
                 out_cap,
                 "%s: %s",
                 prefix ? prefix : "Analysis refresh",
                 status->startup_audit.refresh_reason);
    } else if (status->has_cache) {
        snprintf(out, out_cap, "(cached)");
    }
}
