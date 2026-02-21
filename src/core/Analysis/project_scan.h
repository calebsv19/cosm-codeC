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

#endif // PROJECT_SCAN_H
