#ifndef PROJECT_SCAN_H
#define PROJECT_SCAN_H

// Required for BuildFlagSet
#include "core/Analysis/include_path_resolver.h"
#include <stdbool.h>

// Run a project-wide static analysis sweep over the current workspace.
// Scans for *.c and *.h files (skips ide_files/, build/, and .git/) and
// pushes diagnostics into the global diagnostics engine.
void analysis_scan_workspace(const char* root);
// Variant that reuses a precomputed flag set (include paths/macros).
// If update_engine is true, pushes diagnostics into the global diagnostics engine.
void analysis_scan_workspace_with_flags(const char* root, const BuildFlagSet* flags, bool update_engine);

#endif // PROJECT_SCAN_H
