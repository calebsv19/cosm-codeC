#ifndef ANALYSIS_STORE_H
#define ANALYSIS_STORE_H

#include <stddef.h>
#include <stdint.h>

#include "core/Diagnostics/diagnostics_engine.h"
#include "fisics_frontend.h"

typedef struct {
    char* path;
    Diagnostic* diags;
    int count;
    uint64_t stamp; // recency ordering (higher = newer)
} AnalysisFileDiagnostics;

// Reset all stored per-file diagnostics.
void analysis_store_clear(void);

// Upsert diagnostics for a file, marking it most-recent.
void analysis_store_upsert(const char* filePath,
                           const FisicsDiagnostic* fisicsDiags,
                           size_t diagCount);

// Synchronization helpers (lock around multi-step reads in UI).
void analysis_store_lock(void);
void analysis_store_unlock(void);

// Count of files with diagnostics.
size_t analysis_store_file_count(void);

// Access a file entry by index (recency-ordered: 0 = newest). Returns NULL if out of range.
const AnalysisFileDiagnostics* analysis_store_file_at(size_t idx);

// Flatten current store into the legacy diagnostics_engine (recency-ordered).
void analysis_store_flatten_to_engine(void);

// Persistence: save/load to workspace/ide_files/analysis_diagnostics.json
void analysis_store_save(const char* workspaceRoot);
void analysis_store_load(const char* workspaceRoot);

#endif // ANALYSIS_STORE_H
