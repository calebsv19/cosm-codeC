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
const char* loadFontPresetPreference(void);
void saveFontPresetPreference(const char* preset_name);
int loadFontZoomStepPreference(int* out_step);
void saveFontZoomStepPreference(int step);

/* IDE-local presentation projection. Kept out of shared core until more than
 * one program proves the same persisted pane semantics. */
int loadWorkspaceAuthoringPresentationPreference(int *out_tool_panel_visible,
                                                 int *out_control_panel_visible,
                                                 int *out_terminal_visible,
                                                 int *out_active_tool);
void saveWorkspaceAuthoringPresentationPreference(int tool_panel_visible,
                                                  int control_panel_visible,
                                                  int terminal_visible,
                                                  int active_tool);

const WorkspaceBuildConfig* getWorkspaceBuildConfig(void);
void saveWorkspaceBuildConfig(const WorkspaceBuildConfig* config);
void resetWorkspaceBuildConfigDefaults(void);

#endif // WORKSPACE_PREFS_H
