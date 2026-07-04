#ifndef ANALYSIS_STARTUP_AUDIT_H
#define ANALYSIS_STARTUP_AUDIT_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    ANALYSIS_STARTUP_REFRESH_UNKNOWN = 0,
    ANALYSIS_STARTUP_REFRESH_FULL_REQUIRED,
    ANALYSIS_STARTUP_REFRESH_INCREMENTAL_TARGETS,
    ANALYSIS_STARTUP_REFRESH_INCREMENTAL_VERIFY_NOOP
} AnalysisStartupRefreshIntent;

typedef struct {
    bool valid;
    bool cache_meta_trusted;
    char cache_meta_reason[96];

    bool snapshot_loaded;
    bool current_scan_ok;
    size_t current_file_count;
    size_t cached_file_count;
    size_t hash_matched_count;
    size_t dirty_count;
    size_t removed_count;
    size_t new_count;

    bool diagnostics_ready;
    bool symbols_ready;
    bool units_ready;
    bool tokens_ready;
    bool tokens_required;
    bool library_ready;
    bool include_graph_ready;
    bool build_flags_ready;

    AnalysisStartupRefreshIntent refresh_intent;
    char refresh_reason[128];
} AnalysisStartupAudit;

void analysis_startup_audit_init(AnalysisStartupAudit* audit);
bool analysis_startup_audit_evaluate(const char* workspace_root,
                                     const char* build_args,
                                     AnalysisStartupAudit* out_audit);
const char* analysis_startup_refresh_intent_string(AnalysisStartupRefreshIntent intent);

#endif // ANALYSIS_STARTUP_AUDIT_H
