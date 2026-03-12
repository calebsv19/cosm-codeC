#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/LoopEvents/event_queue.h"

static void test_document_emitters(void) {
    loop_events_init();

    assert(loop_events_emit_document_edited("src/main.c", 9u));
    assert(loop_events_emit_document_revision_changed("src/main.c", 10u));

    IDEEvent ev;
    memset(&ev, 0, sizeof(ev));

    assert(loop_events_pop(&ev));
    assert(ev.type == IDE_EVENT_DOCUMENT_EDITED);
    assert(ev.sequence == 1u);
    assert(strcmp(ev.payload.document.document_path, "src/main.c") == 0);
    assert(ev.payload.document.document_revision == 9u);

    assert(loop_events_pop(&ev));
    assert(ev.type == IDE_EVENT_DOCUMENT_REVISION_CHANGED);
    assert(ev.sequence == 2u);
    assert(strcmp(ev.payload.document.document_path, "src/main.c") == 0);
    assert(ev.payload.document.document_revision == 10u);

    loop_events_shutdown();
}

static void test_analysis_emitters(void) {
    loop_events_init();

    assert(loop_events_emit_symbol_tree_updated("/tmp/project", 77u, 1234u));
    assert(loop_events_emit_diagnostics_updated("/tmp/project", 78u, 5678u));
    assert(loop_events_emit_analysis_progress_updated("/tmp/project", 79u, 901u));
    assert(loop_events_emit_analysis_status_updated("/tmp/project", 80u, 902u));
    assert(loop_events_emit_library_index_updated("/tmp/project", 81u, 903u));
    assert(loop_events_emit_analysis_run_finished("/tmp/project", 82u, 904u));

    IDEEvent ev;
    memset(&ev, 0, sizeof(ev));

    assert(loop_events_pop(&ev));
    assert(ev.type == IDE_EVENT_SYMBOL_TREE_UPDATED);
    assert(ev.sequence == 1u);
    assert(strcmp(ev.payload.analysis.project_root, "/tmp/project") == 0);
    assert(ev.payload.analysis.analysis_run_id == 77u);
    assert(ev.payload.analysis.data_stamp == 1234u);

    assert(loop_events_pop(&ev));
    assert(ev.type == IDE_EVENT_DIAGNOSTICS_UPDATED);
    assert(ev.sequence == 2u);
    assert(strcmp(ev.payload.analysis.project_root, "/tmp/project") == 0);
    assert(ev.payload.analysis.analysis_run_id == 78u);
    assert(ev.payload.analysis.data_stamp == 5678u);

    assert(loop_events_pop(&ev));
    assert(ev.type == IDE_EVENT_ANALYSIS_PROGRESS_UPDATED);
    assert(ev.sequence == 3u);
    assert(ev.payload.analysis.analysis_run_id == 79u);
    assert(ev.payload.analysis.data_stamp == 901u);

    assert(loop_events_pop(&ev));
    assert(ev.type == IDE_EVENT_ANALYSIS_STATUS_UPDATED);
    assert(ev.sequence == 4u);
    assert(ev.payload.analysis.analysis_run_id == 80u);
    assert(ev.payload.analysis.data_stamp == 902u);

    assert(loop_events_pop(&ev));
    assert(ev.type == IDE_EVENT_LIBRARY_INDEX_UPDATED);
    assert(ev.sequence == 5u);
    assert(ev.payload.analysis.analysis_run_id == 81u);
    assert(ev.payload.analysis.data_stamp == 903u);

    assert(loop_events_pop(&ev));
    assert(ev.type == IDE_EVENT_ANALYSIS_RUN_FINISHED);
    assert(ev.sequence == 6u);
    assert(ev.payload.analysis.analysis_run_id == 82u);
    assert(ev.payload.analysis.data_stamp == 904u);

    loop_events_shutdown();
}

int main(void) {
    test_document_emitters();
    test_analysis_emitters();
    puts("loop_events_emission_contract_test: success");
    return 0;
}
