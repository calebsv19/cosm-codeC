#include "workspace_prefs.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static char cachedWorkspacePath[PATH_MAX];
static char cachedRunTargetPath[PATH_MAX];
static WorkspaceBuildConfig cachedBuildConfig;
static int configLoaded = 0;

static int buildConfigPath(char* outPath, size_t pathSize, int includeFile) {
    if (!outPath || pathSize == 0) return 0;

    const char* home = getenv("HOME");
    if (!home || home[0] == '\0') {
        return 0;
    }

    if (includeFile) {
        return snprintf(outPath, pathSize, "%s/%s/%s", home, ".custom_c_ide", "config.ini") < (int)pathSize;
    }

    return snprintf(outPath, pathSize, "%s/%s", home, ".custom_c_ide") < (int)pathSize;
}

static void ensureConfigDirectory(void) {
    char dirPath[PATH_MAX];
    if (!buildConfigPath(dirPath, sizeof(dirPath), 0)) {
        return;
    }

    struct stat st = {0};
    if (stat(dirPath, &st) == 0) {
        return;
    }

    if (mkdir(dirPath, 0700) != 0 && errno != EEXIST) {
        fprintf(stderr, "[WorkspacePrefs] Failed to create config directory: %s (%s)\n",
                dirPath, strerror(errno));
    }
}

void resetWorkspaceBuildConfigDefaults(void) {
    memset(&cachedBuildConfig, 0, sizeof(cachedBuildConfig));
}

static void trimTrailingWhitespace(char* str) {
    if (!str) return;

    size_t len = strlen(str);
    while (len > 0) {
        char c = str[len - 1];
        if (c == '\n' || c == '\r' || c == ' ' || c == '\t') {
            str[len - 1] = '\0';
            len--;
        } else {
            break;
        }
    }
}

static void loadConfigFile(void) {
    if (configLoaded) {
        return;
    }
    configLoaded = 1;

    cachedWorkspacePath[0] = '\0';
    cachedRunTargetPath[0] = '\0';
    resetWorkspaceBuildConfigDefaults();

    char configPath[PATH_MAX];
    if (!buildConfigPath(configPath, sizeof(configPath), 1)) {
        return;
    }

    FILE* file = fopen(configPath, "r");
    if (!file) {
        return;
    }

    char line[PATH_MAX + 512];
    while (fgets(line, sizeof(line), file)) {
        trimTrailingWhitespace(line);
        if (line[0] == '\0') continue;

        const char* equals = strchr(line, '=');
        if (!equals) continue;

        size_t keyLen = (size_t)(equals - line);
        const char* value = equals + 1;

        if (strncmp(line, "workspace", keyLen) == 0) {
            strncpy(cachedWorkspacePath, value, sizeof(cachedWorkspacePath) - 1);
            cachedWorkspacePath[sizeof(cachedWorkspacePath) - 1] = '\0';
        } else if (strncmp(line, "run_target", keyLen) == 0) {
            strncpy(cachedRunTargetPath, value, sizeof(cachedRunTargetPath) - 1);
            cachedRunTargetPath[sizeof(cachedRunTargetPath) - 1] = '\0';
        } else if (strncmp(line, "build_command", keyLen) == 0) {
            strncpy(cachedBuildConfig.build_command, value, sizeof(cachedBuildConfig.build_command) - 1);
            cachedBuildConfig.build_command[sizeof(cachedBuildConfig.build_command) - 1] = '\0';
        } else if (strncmp(line, "build_args", keyLen) == 0) {
            strncpy(cachedBuildConfig.build_args, value, sizeof(cachedBuildConfig.build_args) - 1);
            cachedBuildConfig.build_args[sizeof(cachedBuildConfig.build_args) - 1] = '\0';
        } else if (strncmp(line, "build_workdir", keyLen) == 0) {
            strncpy(cachedBuildConfig.build_working_dir, value, sizeof(cachedBuildConfig.build_working_dir) - 1);
            cachedBuildConfig.build_working_dir[sizeof(cachedBuildConfig.build_working_dir) - 1] = '\0';
        } else if (strncmp(line, "build_output_dir", keyLen) == 0) {
            strncpy(cachedBuildConfig.build_output_dir, value, sizeof(cachedBuildConfig.build_output_dir) - 1);
            cachedBuildConfig.build_output_dir[sizeof(cachedBuildConfig.build_output_dir) - 1] = '\0';
        } else if (strncmp(line, "run_command", keyLen) == 0) {
            strncpy(cachedBuildConfig.run_command, value, sizeof(cachedBuildConfig.run_command) - 1);
            cachedBuildConfig.run_command[sizeof(cachedBuildConfig.run_command) - 1] = '\0';
        } else if (strncmp(line, "run_args", keyLen) == 0) {
            strncpy(cachedBuildConfig.run_args, value, sizeof(cachedBuildConfig.run_args) - 1);
            cachedBuildConfig.run_args[sizeof(cachedBuildConfig.run_args) - 1] = '\0';
        } else if (strncmp(line, "run_workdir", keyLen) == 0) {
            strncpy(cachedBuildConfig.run_working_dir, value, sizeof(cachedBuildConfig.run_working_dir) - 1);
            cachedBuildConfig.run_working_dir[sizeof(cachedBuildConfig.run_working_dir) - 1] = '\0';
        }
    }

    fclose(file);
}

static void writeConfigFile(void) {
    ensureConfigDirectory();

    char configPath[PATH_MAX];
    if (!buildConfigPath(configPath, sizeof(configPath), 1)) {
        return;
    }

    FILE* file = fopen(configPath, "w");
    if (!file) {
        fprintf(stderr, "[WorkspacePrefs] Failed to write config file: %s (%s)\n",
                configPath, strerror(errno));
        return;
    }

    fprintf(file, "workspace=%s\n", cachedWorkspacePath);
    fprintf(file, "run_target=%s\n", cachedRunTargetPath);
    fprintf(file, "build_command=%s\n", cachedBuildConfig.build_command);
    fprintf(file, "build_args=%s\n", cachedBuildConfig.build_args);
    fprintf(file, "build_workdir=%s\n", cachedBuildConfig.build_working_dir);
    fprintf(file, "build_output_dir=%s\n", cachedBuildConfig.build_output_dir);
    fprintf(file, "run_command=%s\n", cachedBuildConfig.run_command);
    fprintf(file, "run_args=%s\n", cachedBuildConfig.run_args);
    fprintf(file, "run_workdir=%s\n", cachedBuildConfig.run_working_dir);

    fclose(file);
}

const char* loadWorkspacePreference(void) {
    loadConfigFile();
    return (cachedWorkspacePath[0] != '\0') ? cachedWorkspacePath : NULL;
}

void saveWorkspacePreference(const char* path) {
    loadConfigFile();
    if (path && *path) {
        strncpy(cachedWorkspacePath, path, sizeof(cachedWorkspacePath) - 1);
        cachedWorkspacePath[sizeof(cachedWorkspacePath) - 1] = '\0';
    } else {
        cachedWorkspacePath[0] = '\0';
    }
    writeConfigFile();
}

const char* loadRunTargetPreference(void) {
    loadConfigFile();
    return (cachedRunTargetPath[0] != '\0') ? cachedRunTargetPath : NULL;
}

void saveRunTargetPreference(const char* path) {
    loadConfigFile();
    if (path && *path) {
        strncpy(cachedRunTargetPath, path, sizeof(cachedRunTargetPath) - 1);
        cachedRunTargetPath[sizeof(cachedRunTargetPath) - 1] = '\0';
    } else {
        cachedRunTargetPath[0] = '\0';
    }
    writeConfigFile();
}

const WorkspaceBuildConfig* getWorkspaceBuildConfig(void) {
    loadConfigFile();
    return &cachedBuildConfig;
}

void saveWorkspaceBuildConfig(const WorkspaceBuildConfig* config) {
    loadConfigFile();
    if (!config) return;

    memcpy(&cachedBuildConfig, config, sizeof(cachedBuildConfig));
    writeConfigFile();
}
