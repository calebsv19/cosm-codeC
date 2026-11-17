#ifndef PROJECT_SCAN_H
#define PROJECT_SCAN_H

// Required for BuildFlagSet
#include "core/Analysis/include_path_resolver.h"
#include <stddef.h>
#include <stdbool.h>

// Run a project-wide static analysis sweep over the current workspace.
// Scans for *.c and *.h files (skips ide_files/, build/, and .git/) and
// pushes diagnostics into the global diagnostics engine.
void analysis_scan_workspace(const char* root);
// Variant that reuses a precomputed flag set (include paths/macros).
// If update_engine is true, pushes diagnostics into the global diagnostics engine.
void analysis_scan_workspace_with_flags(const char* root, const BuildFlagSet* flags, bool update_engine);

// Incremental variant: analyzes only the provided absolute file paths.
// - root: workspace root used for persistence outputs.
// - files: list of file paths to analyze.
// - file_count: number of paths in files.
// - update_engine: if true, refreshes legacy diagnostics engine after updates.
// - persist_outputs: if true, writes diagnostics/symbols/tokens JSON artifacts.
void analysis_scan_files_with_flags(const char* root,
                                    const char* const* files,
                                    size_t file_count,
                                    const BuildFlagSet* flags,
                                    bool update_engine,
                                    bool persist_outputs);

// The live compiler owns the input bytes until the guarded terminal result is released.
typedef struct AnalysisPreparedBuffer AnalysisPreparedBuffer;
AnalysisPreparedBuffer* analysis_prepare_live_buffer(const char* file_path, char* owned_contents,
                                                     size_t length, const BuildFlagSet* flags);
void analysis_publish_live_buffer(const char* root, const char* file_path,
                                  const AnalysisPreparedBuffer* prepared);
void analysis_release_live_buffer(void* payload);

#endif // PROJECT_SCAN_H
