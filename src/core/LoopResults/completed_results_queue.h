#ifndef COMPLETED_RESULTS_QUEUE_H
#define COMPLETED_RESULTS_QUEUE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum CompletedResultSubsystem {
    COMPLETED_SUBSYSTEM_NONE = 0,
    COMPLETED_SUBSYSTEM_ANALYSIS = 1,
    COMPLETED_SUBSYSTEM_SYMBOLS = 2,
    COMPLETED_SUBSYSTEM_DIAGNOSTICS = 3
} CompletedResultSubsystem;

typedef enum CompletedResultKind {
    COMPLETED_RESULT_NONE = 0,
    COMPLETED_RESULT_ANALYSIS_FINISHED = 1,
    COMPLETED_RESULT_SYMBOLS_UPDATED = 2,
    COMPLETED_RESULT_DIAGNOSTICS_UPDATED = 3,
    COMPLETED_RESULT_ANALYSIS_PROGRESS = 4,
    COMPLETED_RESULT_ANALYSIS_STATUS_UPDATE = 5
} CompletedResultKind;

typedef struct CompletedResultAnalysisFinishedPayload {
    uint64_t analysis_run_id;
    bool cancelled;
    bool had_error;
    uint64_t library_index_stamp;
    char project_root[1024];
} CompletedResultAnalysisFinishedPayload;

typedef struct CompletedResultSymbolsUpdatedPayload {
    uint64_t analysis_run_id;
    char project_root[1024];
    uint64_t symbols_stamp;
} CompletedResultSymbolsUpdatedPayload;

typedef struct CompletedResultDiagnosticsUpdatedPayload {
    uint64_t analysis_run_id;
    char project_root[1024];
    uint64_t diagnostics_stamp;
} CompletedResultDiagnosticsUpdatedPayload;

typedef struct CompletedResultAnalysisProgressPayload {
    uint64_t analysis_run_id;
    char project_root[1024];
    int completed_files;
    int total_files;
} CompletedResultAnalysisProgressPayload;

typedef struct CompletedResultAnalysisStatusUpdatePayload {
    uint64_t analysis_run_id;
    char project_root[1024];
    bool set_status;
    int status_value;
    bool set_has_cache;
    bool has_cache;
    bool set_last_error;
    char last_error[256];
} CompletedResultAnalysisStatusUpdatePayload;

typedef struct CompletedResult {
    CompletedResultSubsystem subsystem;
    CompletedResultKind kind;
    uint64_t seq;
    bool has_document_revision;
    char document_path[1024];
    uint64_t document_revision;
    void* owned_ptr;
    void (*free_owned_ptr)(void*);
    union {
        CompletedResultAnalysisFinishedPayload analysis_finished;
        CompletedResultSymbolsUpdatedPayload symbols_updated;
        CompletedResultDiagnosticsUpdatedPayload diagnostics_updated;
        CompletedResultAnalysisProgressPayload analysis_progress;
        CompletedResultAnalysisStatusUpdatePayload analysis_status_update;
    } payload;
} CompletedResult;

typedef struct CompletedResultsQueueStats {
    uint32_t total_depth;
    uint32_t analysis_depth;
    uint32_t symbols_depth;
    uint32_t diagnostics_depth;
    uint32_t high_watermark;
    uint64_t pushed;
    uint64_t popped;
    uint64_t results_applied;
    uint64_t results_stale_dropped;
} CompletedResultsQueueStats;

void completed_results_queue_init(void);
void completed_results_queue_shutdown(void);
void completed_results_queue_reset(void);

bool completed_results_queue_push(const CompletedResult* result);
bool completed_results_queue_pop(CompletedResultSubsystem subsystem, CompletedResult* out);
bool completed_results_queue_pop_any(CompletedResult* out);
void completed_results_queue_release(CompletedResult* result);

void completed_results_queue_note_applied(void);
void completed_results_queue_note_stale_dropped(void);

void completed_results_queue_snapshot(CompletedResultsQueueStats* out);

#endif // COMPLETED_RESULTS_QUEUE_H
