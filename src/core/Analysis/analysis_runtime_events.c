#include "core/Analysis/analysis_runtime_events.h"

#include "core/Analysis/analysis_store.h"
#include "core/Analysis/analysis_symbols_store.h"
#include "core/LoopEvents/event_queue.h"

bool analysis_emit_store_hydrated_events(const char* project_root) {
    // Keep diagnostics projection in sync with authoritative analysis_store.
    analysis_store_flatten_to_engine();

    bool ok_diag = loop_events_emit_diagnostics_updated(project_root,
                                                        0u,
                                                        analysis_store_combined_stamp());
    bool ok_symbols = loop_events_emit_symbol_tree_updated(project_root,
                                                           0u,
                                                           analysis_symbols_store_combined_stamp());
    return ok_diag && ok_symbols;
}
