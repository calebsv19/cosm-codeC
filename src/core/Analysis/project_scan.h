#ifndef PROJECT_SCAN_H
#define PROJECT_SCAN_H

// Run a project-wide static analysis sweep over the current workspace.
// Scans for *.c and *.h files (skips ide_files/, build/, and .git/) and
// pushes diagnostics into the global diagnostics engine.
void analysis_scan_workspace(const char* root);

#endif // PROJECT_SCAN_H
