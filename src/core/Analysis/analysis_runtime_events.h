#ifndef ANALYSIS_RUNTIME_EVENTS_H
#define ANALYSIS_RUNTIME_EVENTS_H

#include <stdbool.h>

// Emits startup/store-hydration analysis runtime events in deterministic order.
// Returns true when both diagnostics and symbols events were enqueued.
bool analysis_emit_store_hydrated_events(const char* project_root);

#endif // ANALYSIS_RUNTIME_EVENTS_H
