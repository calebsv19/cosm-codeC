#ifndef WORKSPACE_PREFS_H
#define WORKSPACE_PREFS_H

// Placeholder persistence hooks for workspace selection.
// These will be filled in with real config IO in a future iteration.

const char* loadWorkspacePreference(void);
void saveWorkspacePreference(const char* path);
const char* loadRunTargetPreference(void);
void saveRunTargetPreference(const char* path);

#endif // WORKSPACE_PREFS_H
