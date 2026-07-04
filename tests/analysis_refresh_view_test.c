#include "core/Analysis/analysis_refresh_view.h"

#include <stdio.h>
#include <string.h>

void analysis_status_snapshot(AnalysisStatusSnapshot* out) {
    if (out) {
        memset(out, 0, sizeof(*out));
    }
}

void analysis_status_get_progress(int* out_completed_files, int* out_total_files) {
    if (out_completed_files) {
        *out_completed_files = 0;
    }
    if (out_total_files) {
        *out_total_files = 0;
    }
}

void analysis_scheduler_snapshot(AnalysisSchedulerSnapshot* out) {
    if (out) {
        memset(out, 0, sizeof(*out));
    }
}

static int expect_text(const AnalysisRefreshViewSnapshot* view, const char* expected) {
    char buf[128] = {0};
    analysis_refresh_view_format_status_text(view, buf, sizeof(buf));
    if (strcmp(buf, expected) != 0) {
        fprintf(stderr, "expected '%s', got '%s'\n", expected, buf);
        return 1;
    }
    return 0;
}

int main(void) {
    AnalysisRefreshViewSnapshot view = {0};

    view.status.updating = true;
    view.progress_completed = 3;
    view.progress_total = 10;
    view.scheduler.active_run_id = 42;
    if (expect_text(&view, "Updating 3/10 (#42)")) return 1;

    view.scheduler.pending_editor_save_diagnostics = true;
    if (expect_text(&view, "Updating 3/10 (#42) + save queued")) return 1;

    memset(&view, 0, sizeof(view));
    view.status.updating = true;
    view.scheduler.active_run_id = 7;
    if (expect_text(&view, "Updating (#7)")) return 1;

    memset(&view, 0, sizeof(view));
    view.scheduler.pending_editor_save_diagnostics = true;
    if (expect_text(&view, "Save diagnostics queued")) return 1;

    memset(&view, 0, sizeof(view));
    snprintf(view.status.last_error, sizeof(view.status.last_error), "boom");
    if (expect_text(&view, "Analysis error: boom")) return 1;

    memset(&view, 0, sizeof(view));
    view.status.status = ANALYSIS_STATUS_STALE_LOADING;
    view.status.has_startup_audit = true;
    view.status.startup_audit.valid = true;
    view.status.startup_audit.refresh_intent = ANALYSIS_STARTUP_REFRESH_FULL_REQUIRED;
    snprintf(view.status.startup_audit.refresh_reason,
             sizeof(view.status.startup_audit.refresh_reason),
             "cache_meta_missing");
    if (expect_text(&view, "Analysis refresh needed: cache_meta_missing")) return 1;

    view.status.startup_audit.refresh_intent = ANALYSIS_STARTUP_REFRESH_INCREMENTAL_TARGETS;
    snprintf(view.status.startup_audit.refresh_reason,
             sizeof(view.status.startup_audit.refresh_reason),
             "source_hash_changes_detected");
    if (expect_text(&view, "Analysis refresh queued: source_hash_changes_detected")) return 1;

    memset(&view, 0, sizeof(view));
    view.status.has_cache = true;
    if (expect_text(&view, "(cached)")) return 1;

    memset(&view, 0, sizeof(view));
    char buf[16] = "unchanged";
    analysis_refresh_view_format_status_text(&view, buf, sizeof(buf));
    if (buf[0] != '\0') {
        fprintf(stderr, "expected empty idle status, got '%s'\n", buf);
        return 1;
    }

    puts("analysis_refresh_view_test: ok");
    return 0;
}
