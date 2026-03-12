#include "core/LoopEvents/event_queue.h"

#include <stdio.h>
#include <string.h>

#include "core/LoopKernel/mainthread_context.h"

enum { LOOP_EVENTS_QUEUE_CAPACITY = 512 };

static IDEEvent g_events[LOOP_EVENTS_QUEUE_CAPACITY];
static size_t g_head = 0;
static size_t g_tail = 0;
static size_t g_count = 0;

static bool g_initialized = false;

static uint64_t g_next_sequence = 1;
static uint32_t g_high_watermark = 0;
static uint64_t g_events_enqueued = 0;
static uint64_t g_events_processed = 0;
static uint64_t g_events_deferred = 0;
static uint64_t g_events_dropped_overflow = 0;

void loop_events_reset(void) {
    memset(g_events, 0, sizeof(g_events));
    g_head = 0;
    g_tail = 0;
    g_count = 0;
    g_next_sequence = 1;
    g_high_watermark = 0;
    g_events_enqueued = 0;
    g_events_processed = 0;
    g_events_deferred = 0;
    g_events_dropped_overflow = 0;
}

void loop_events_init(void) {
    g_initialized = true;
    loop_events_reset();
}

void loop_events_shutdown(void) {
    loop_events_reset();
    g_initialized = false;
}

bool loop_events_push(const IDEEvent* event) {
    mainthread_context_assert_owner("loop_events.push");
    if (!g_initialized || !event || event->type == IDE_EVENT_NONE) return false;
    if (g_count >= LOOP_EVENTS_QUEUE_CAPACITY) {
        g_events_dropped_overflow++;
        return false;
    }

    IDEEvent next = *event;
    next.sequence = g_next_sequence++;
    g_events[g_tail] = next;
    g_tail = (g_tail + 1u) % LOOP_EVENTS_QUEUE_CAPACITY;
    g_count++;
    g_events_enqueued++;
    if ((uint32_t)g_count > g_high_watermark) {
        g_high_watermark = (uint32_t)g_count;
    }
    return true;
}

bool loop_events_pop(IDEEvent* out) {
    if (!g_initialized || !out || g_count == 0u) return false;

    *out = g_events[g_head];
    memset(&g_events[g_head], 0, sizeof(g_events[g_head]));
    g_head = (g_head + 1u) % LOOP_EVENTS_QUEUE_CAPACITY;
    g_count--;
    g_events_processed++;
    return true;
}

size_t loop_events_size(void) {
    if (!g_initialized) return 0u;
    return g_count;
}

size_t loop_events_capacity(void) {
    return LOOP_EVENTS_QUEUE_CAPACITY;
}

void loop_events_note_deferred(uint64_t count) {
    g_events_deferred += count;
}

size_t loop_events_drain_bounded(size_t budget, LoopEventVisitorFn visitor, void* user_data) {
    if (!g_initialized) return 0u;
    size_t drained = 0u;
    IDEEvent event;
    while (drained < budget && loop_events_pop(&event)) {
        if (visitor) {
            visitor(&event, user_data);
        }
        drained++;
    }
    if (g_count > 0u) {
        loop_events_note_deferred((uint64_t)g_count);
    }
    return drained;
}

void loop_events_snapshot(LoopEventsStats* out) {
    if (!out) return;
    out->depth = (uint32_t)g_count;
    out->high_watermark = g_high_watermark;
    out->events_enqueued = g_events_enqueued;
    out->events_processed = g_events_processed;
    out->events_deferred = g_events_deferred;
    out->events_dropped_overflow = g_events_dropped_overflow;
    out->next_sequence = g_next_sequence;
}

static void copy_text_field(char* dst, size_t dst_cap, const char* src) {
    if (!dst || dst_cap == 0u) return;
    dst[0] = '\0';
    if (!src || !src[0]) return;
    snprintf(dst, dst_cap, "%s", src);
}

bool loop_events_emit_document_edited(const char* document_path, uint64_t document_revision) {
    IDEEvent event;
    memset(&event, 0, sizeof(event));
    event.type = IDE_EVENT_DOCUMENT_EDITED;
    copy_text_field(event.payload.document.document_path,
                    sizeof(event.payload.document.document_path),
                    document_path);
    event.payload.document.document_revision = document_revision;
    return loop_events_push(&event);
}

bool loop_events_emit_document_revision_changed(const char* document_path, uint64_t document_revision) {
    IDEEvent event;
    memset(&event, 0, sizeof(event));
    event.type = IDE_EVENT_DOCUMENT_REVISION_CHANGED;
    copy_text_field(event.payload.document.document_path,
                    sizeof(event.payload.document.document_path),
                    document_path);
    event.payload.document.document_revision = document_revision;
    return loop_events_push(&event);
}

bool loop_events_emit_symbol_tree_updated(const char* project_root,
                                          uint64_t analysis_run_id,
                                          uint64_t symbols_stamp) {
    IDEEvent event;
    memset(&event, 0, sizeof(event));
    event.type = IDE_EVENT_SYMBOL_TREE_UPDATED;
    copy_text_field(event.payload.analysis.project_root,
                    sizeof(event.payload.analysis.project_root),
                    project_root);
    event.payload.analysis.analysis_run_id = analysis_run_id;
    event.payload.analysis.data_stamp = symbols_stamp;
    return loop_events_push(&event);
}

bool loop_events_emit_diagnostics_updated(const char* project_root,
                                          uint64_t analysis_run_id,
                                          uint64_t diagnostics_stamp) {
    IDEEvent event;
    memset(&event, 0, sizeof(event));
    event.type = IDE_EVENT_DIAGNOSTICS_UPDATED;
    copy_text_field(event.payload.analysis.project_root,
                    sizeof(event.payload.analysis.project_root),
                    project_root);
    event.payload.analysis.analysis_run_id = analysis_run_id;
    event.payload.analysis.data_stamp = diagnostics_stamp;
    return loop_events_push(&event);
}

bool loop_events_emit_analysis_progress_updated(const char* project_root,
                                                uint64_t analysis_run_id,
                                                uint64_t progress_stamp) {
    IDEEvent event;
    memset(&event, 0, sizeof(event));
    event.type = IDE_EVENT_ANALYSIS_PROGRESS_UPDATED;
    copy_text_field(event.payload.analysis.project_root,
                    sizeof(event.payload.analysis.project_root),
                    project_root);
    event.payload.analysis.analysis_run_id = analysis_run_id;
    event.payload.analysis.data_stamp = progress_stamp;
    return loop_events_push(&event);
}

bool loop_events_emit_analysis_status_updated(const char* project_root,
                                              uint64_t analysis_run_id,
                                              uint64_t status_stamp) {
    IDEEvent event;
    memset(&event, 0, sizeof(event));
    event.type = IDE_EVENT_ANALYSIS_STATUS_UPDATED;
    copy_text_field(event.payload.analysis.project_root,
                    sizeof(event.payload.analysis.project_root),
                    project_root);
    event.payload.analysis.analysis_run_id = analysis_run_id;
    event.payload.analysis.data_stamp = status_stamp;
    return loop_events_push(&event);
}

bool loop_events_emit_library_index_updated(const char* project_root,
                                            uint64_t analysis_run_id,
                                            uint64_t index_stamp) {
    IDEEvent event;
    memset(&event, 0, sizeof(event));
    event.type = IDE_EVENT_LIBRARY_INDEX_UPDATED;
    copy_text_field(event.payload.analysis.project_root,
                    sizeof(event.payload.analysis.project_root),
                    project_root);
    event.payload.analysis.analysis_run_id = analysis_run_id;
    event.payload.analysis.data_stamp = index_stamp;
    return loop_events_push(&event);
}

bool loop_events_emit_analysis_run_finished(const char* project_root,
                                            uint64_t analysis_run_id,
                                            uint64_t index_stamp) {
    IDEEvent event;
    memset(&event, 0, sizeof(event));
    event.type = IDE_EVENT_ANALYSIS_RUN_FINISHED;
    copy_text_field(event.payload.analysis.project_root,
                    sizeof(event.payload.analysis.project_root),
                    project_root);
    event.payload.analysis.analysis_run_id = analysis_run_id;
    event.payload.analysis.data_stamp = index_stamp;
    return loop_events_push(&event);
}
