#ifndef EVENT_QUEUE_H
#define EVENT_QUEUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum IDEEventType {
    IDE_EVENT_NONE = 0,
    IDE_EVENT_DOCUMENT_EDITED = 1,
    IDE_EVENT_DOCUMENT_REVISION_CHANGED = 2,
    IDE_EVENT_SYMBOL_TREE_UPDATED = 3,
    IDE_EVENT_DIAGNOSTICS_UPDATED = 4,
    IDE_EVENT_ANALYSIS_PROGRESS_UPDATED = 5,
    IDE_EVENT_ANALYSIS_STATUS_UPDATED = 6,
    IDE_EVENT_LIBRARY_INDEX_UPDATED = 7,
    IDE_EVENT_ANALYSIS_RUN_FINISHED = 8
} IDEEventType;

typedef struct IDEEventDocumentPayload {
    char document_path[1024];
    uint64_t document_revision;
} IDEEventDocumentPayload;

typedef struct IDEEventAnalysisPayload {
    char project_root[1024];
    uint64_t analysis_run_id;
    uint64_t data_stamp;
} IDEEventAnalysisPayload;

typedef struct IDEEvent {
    IDEEventType type;
    uint64_t sequence;
    union {
        IDEEventDocumentPayload document;
        IDEEventAnalysisPayload analysis;
    } payload;
} IDEEvent;

typedef struct LoopEventsStats {
    uint32_t depth;
    uint32_t high_watermark;
    uint64_t events_enqueued;
    uint64_t events_processed;
    uint64_t events_deferred;
    uint64_t events_dropped_overflow;
    uint64_t next_sequence;
} LoopEventsStats;

typedef void (*LoopEventVisitorFn)(const IDEEvent* event, void* user_data);

void loop_events_init(void);
void loop_events_shutdown(void);
void loop_events_reset(void);

bool loop_events_push(const IDEEvent* event);
bool loop_events_pop(IDEEvent* out);

size_t loop_events_size(void);
size_t loop_events_capacity(void);
void loop_events_note_deferred(uint64_t count);
size_t loop_events_drain_bounded(size_t budget, LoopEventVisitorFn visitor, void* user_data);

void loop_events_snapshot(LoopEventsStats* out);

bool loop_events_emit_document_edited(const char* document_path, uint64_t document_revision);
bool loop_events_emit_document_revision_changed(const char* document_path, uint64_t document_revision);
bool loop_events_emit_symbol_tree_updated(const char* project_root,
                                          uint64_t analysis_run_id,
                                          uint64_t symbols_stamp);
bool loop_events_emit_diagnostics_updated(const char* project_root,
                                          uint64_t analysis_run_id,
                                          uint64_t diagnostics_stamp);
bool loop_events_emit_analysis_progress_updated(const char* project_root,
                                                uint64_t analysis_run_id,
                                                uint64_t progress_stamp);
bool loop_events_emit_analysis_status_updated(const char* project_root,
                                              uint64_t analysis_run_id,
                                              uint64_t status_stamp);
bool loop_events_emit_library_index_updated(const char* project_root,
                                            uint64_t analysis_run_id,
                                            uint64_t index_stamp);
bool loop_events_emit_analysis_run_finished(const char* project_root,
                                            uint64_t analysis_run_id,
                                            uint64_t index_stamp);

#endif // EVENT_QUEUE_H
