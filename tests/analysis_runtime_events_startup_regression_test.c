#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/Analysis/analysis_runtime_events.h"
#include "core/Analysis/analysis_store.h"
#include "core/Analysis/analysis_symbols_store.h"
#include "core/Diagnostics/diagnostics_engine.h"
#include "core/LoopEvents/event_queue.h"

int main(void) {
    const char* project_root = "/tmp/analysis_runtime_events_startup_regression";
    const char* file_path = "/tmp/analysis_runtime_events_startup_regression/src/main.c";

    loop_events_init();
    analysis_store_clear();
    analysis_symbols_store_clear();
    clearDiagnostics();

    FisicsDiagnostic diag = {0};
    diag.file_path = file_path;
    diag.line = 3;
    diag.column = 2;
    diag.length = 4;
    diag.kind = DIAG_WARNING;
    diag.message = "startup diag";
    analysis_store_upsert(file_path, &diag, 1u);

    FisicsSymbol symbol = {0};
    symbol.name = "startup_fn";
    symbol.file_path = file_path;
    symbol.start_line = 3;
    symbol.start_col = 1;
    symbol.end_line = 3;
    symbol.end_col = 12;
    symbol.kind = FISICS_SYMBOL_FUNCTION;
    symbol.is_definition = true;
    symbol.return_type = "int";
    analysis_symbols_store_upsert(file_path, &symbol, 1u);

    uint64_t expected_diag_stamp = analysis_store_combined_stamp();
    uint64_t expected_symbol_stamp = analysis_symbols_store_combined_stamp();
    assert(analysis_store_file_count() == 1u);
    assert(analysis_symbols_store_file_count() == 1u);

    assert(analysis_emit_store_hydrated_events(project_root));
    assert(getDiagnosticCount() == 1);

    IDEEvent ev1 = {0};
    IDEEvent ev2 = {0};
    IDEEvent extra = {0};
    assert(loop_events_pop(&ev1));
    assert(loop_events_pop(&ev2));
    assert(!loop_events_pop(&extra));

    assert(ev1.type == IDE_EVENT_DIAGNOSTICS_UPDATED);
    assert(strcmp(ev1.payload.analysis.project_root, project_root) == 0);
    assert(ev1.payload.analysis.data_stamp == expected_diag_stamp);

    assert(ev2.type == IDE_EVENT_SYMBOL_TREE_UPDATED);
    assert(strcmp(ev2.payload.analysis.project_root, project_root) == 0);
    assert(ev2.payload.analysis.data_stamp == expected_symbol_stamp);

    assert(ev1.sequence < ev2.sequence);

    analysis_store_clear();
    analysis_symbols_store_clear();
    loop_events_shutdown();
    clearDiagnostics();

    printf("analysis_runtime_events_startup_regression_test: success\n");
    return 0;
}
