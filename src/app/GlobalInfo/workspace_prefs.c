#include "workspace_prefs.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static char cachedWorkspacePath[PATH_MAX];
static char cachedRunTargetPath[PATH_MAX];
static char cachedThemePreset[128];
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

static void warnOnWeakConfigFile(const char* configPath, FILE* file) {
    if (!configPath || !file) return;

    struct stat st = {0};
    if (fstat(fileno(file), &st) != 0) {
        return;
    }

    uid_t expectedUid = geteuid();
    if (st.st_uid != expectedUid) {
        fprintf(stderr,
                "[WorkspacePrefs] Warning: config file owner mismatch for %s (owner=%u expected=%u)\n",
                configPath,
                (unsigned)st.st_uid,
                (unsigned)expectedUid);
    }

    mode_t weakBits = st.st_mode & (S_IRWXG | S_IRWXO);
    if (weakBits != 0) {
        fprintf(stderr,
                "[WorkspacePrefs] Warning: config file %s has weak permissions (%03o); expected 600.\n",
                configPath,
                (unsigned)(st.st_mode & 0777));
    }
}

static int writeConfigContents(FILE* file) {
    if (!file) return 0;

    int rc = 1;
    rc &= fprintf(file, "workspace=%s\n", cachedWorkspacePath) >= 0;
    rc &= fprintf(file, "run_target=%s\n", cachedRunTargetPath) >= 0;
    rc &= fprintf(file, "theme_preset=%s\n", cachedThemePreset) >= 0;
    rc &= fprintf(file, "build_command=%s\n", cachedBuildConfig.build_command) >= 0;
    rc &= fprintf(file, "build_args=%s\n", cachedBuildConfig.build_args) >= 0;
    rc &= fprintf(file, "build_workdir=%s\n", cachedBuildConfig.build_working_dir) >= 0;
    rc &= fprintf(file, "build_output_dir=%s\n", cachedBuildConfig.build_output_dir) >= 0;
    rc &= fprintf(file, "run_command=%s\n", cachedBuildConfig.run_command) >= 0;
    rc &= fprintf(file, "run_args=%s\n", cachedBuildConfig.run_args) >= 0;
    rc &= fprintf(file, "run_workdir=%s\n", cachedBuildConfig.run_working_dir) >= 0;
    return rc;
}

static void fsyncConfigDirectory(void) {
    char dirPath[PATH_MAX];
    if (!buildConfigPath(dirPath, sizeof(dirPath), 0)) {
        return;
    }

    int dirFd = open(dirPath, O_RDONLY);
    if (dirFd < 0) {
        return;
    }

    (void)fsync(dirFd);
    close(dirFd);
}

static void loadConfigFile(void) {
    if (configLoaded) {
        return;
    }
    configLoaded = 1;

    cachedWorkspacePath[0] = '\0';
    cachedRunTargetPath[0] = '\0';
    cachedThemePreset[0] = '\0';
    resetWorkspaceBuildConfigDefaults();

    char configPath[PATH_MAX];
    if (!buildConfigPath(configPath, sizeof(configPath), 1)) {
        return;
    }

    FILE* file = fopen(configPath, "r");
    if (!file) {
        return;
    }

    warnOnWeakConfigFile(configPath, file);

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
        } else if (strncmp(line, "theme_preset", keyLen) == 0) {
            strncpy(cachedThemePreset, value, sizeof(cachedThemePreset) - 1);
            cachedThemePreset[sizeof(cachedThemePreset) - 1] = '\0';
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

    char dirPath[PATH_MAX];
    if (!buildConfigPath(dirPath, sizeof(dirPath), 0)) {
        return;
    }

    char configPath[PATH_MAX];
    if (!buildConfigPath(configPath, sizeof(configPath), 1)) {
        return;
    }

    char tempPath[PATH_MAX];
    if (snprintf(tempPath, sizeof(tempPath), "%s/%s", dirPath, "config.ini.tmp.XXXXXX") >= (int)sizeof(tempPath)) {
        fprintf(stderr, "[WorkspacePrefs] Failed to build temp config path.\n");
        return;
    }

    int fd = mkstemp(tempPath);
    if (fd < 0) {
        fprintf(stderr, "[WorkspacePrefs] Failed to create temp config file: %s (%s)\n",
                tempPath, strerror(errno));
        return;
    }

    if (fchmod(fd, 0600) != 0) {
        fprintf(stderr, "[WorkspacePrefs] Failed to set temp config mode: %s (%s)\n",
                tempPath, strerror(errno));
        close(fd);
        unlink(tempPath);
        return;
    }

    FILE* file = fdopen(fd, "w");
    if (!file) {
        fprintf(stderr, "[WorkspacePrefs] Failed to open temp config file: %s (%s)\n",
                tempPath, strerror(errno));
        close(fd);
        unlink(tempPath);
        return;
    }

    int ok = writeConfigContents(file);
    if (!ok || fflush(file) != 0 || fsync(fd) != 0 || fclose(file) != 0) {
        fprintf(stderr, "[WorkspacePrefs] Failed to persist temp config file: %s (%s)\n",
                tempPath, strerror(errno));
        unlink(tempPath);
        return;
    }

    if (rename(tempPath, configPath) != 0) {
        fprintf(stderr, "[WorkspacePrefs] Failed to replace config file: %s (%s)\n",
                configPath, strerror(errno));
        unlink(tempPath);
        return;
    }

    fsyncConfigDirectory();
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

const char* loadThemePresetPreference(void) {
    loadConfigFile();
    return (cachedThemePreset[0] != '\0') ? cachedThemePreset : NULL;
}

void saveThemePresetPreference(const char* preset_name) {
    loadConfigFile();
    if (preset_name && *preset_name) {
        strncpy(cachedThemePreset, preset_name, sizeof(cachedThemePreset) - 1);
        cachedThemePreset[sizeof(cachedThemePreset) - 1] = '\0';
    } else {
        cachedThemePreset[0] = '\0';
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
