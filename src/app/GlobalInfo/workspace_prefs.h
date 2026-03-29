#ifndef WORKSPACE_PREFS_H
#define WORKSPACE_PREFS_H

#include <limits.h>

typedef struct WorkspaceBuildConfig {
    char build_command[256];
    char build_args[512];
    char build_working_dir[PATH_MAX];
    char build_output_dir[PATH_MAX];
    char run_command[256];
    char run_args[512];
    char run_working_dir[PATH_MAX];
} WorkspaceBuildConfig;

const char* loadWorkspacePreference(void);
void saveWorkspacePreference(const char* path);
const char* loadRunTargetPreference(void);
void saveRunTargetPreference(const char* path);
const char* loadThemePresetPreference(void);
void saveThemePresetPreference(const char* preset_name);
int loadFontZoomStepPreference(int* out_step);
void saveFontZoomStepPreference(int step);

const WorkspaceBuildConfig* getWorkspaceBuildConfig(void);
void saveWorkspaceBuildConfig(const WorkspaceBuildConfig* config);
void resetWorkspaceBuildConfigDefaults(void);

#endif // WORKSPACE_PREFS_H
