#include "app/GlobalInfo/core_state.h"
#include "workspace_prefs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>

#include "core_state.h"

static char cachedWorkspacePath[PATH_MAX];
static char cachedRunTargetPath[PATH_MAX];

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
        return; // exists already
    }

    if (mkdir(dirPath, 0700) != 0 && errno != EEXIST) {
        fprintf(stderr, "[WorkspacePrefs] Failed to create config directory: %s (%s)\n",
                dirPath, strerror(errno));
    }
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

const char* loadWorkspacePreference(void) {
    if (cachedWorkspacePath[0] != '\0') {
        return cachedWorkspacePath;
    }

    char configPath[PATH_MAX];
    if (!buildConfigPath(configPath, sizeof(configPath), 1)) {
        return NULL;
    }

    FILE* file = fopen(configPath, "r");
    if (!file) {
        return NULL;
    }

    char line[PATH_MAX + 32];
    while (fgets(line, sizeof(line), file)) {
        trimTrailingWhitespace(line);

        const char* prefixWorkspace = "workspace=";
        const char* prefixRun = "run_target=";
        size_t workspaceLen = strlen(prefixWorkspace);
        size_t runLen = strlen(prefixRun);

        if (strncmp(line, prefixWorkspace, workspaceLen) == 0) {
            const char* value = line + workspaceLen;
            if (*value != '\0') {
                strncpy(cachedWorkspacePath, value, sizeof(cachedWorkspacePath) - 1);
                cachedWorkspacePath[sizeof(cachedWorkspacePath) - 1] = '\0';
            }
        } else if (strncmp(line, prefixRun, runLen) == 0) {
            const char* value = line + runLen;
            if (*value != '\0') {
                strncpy(cachedRunTargetPath, value, sizeof(cachedRunTargetPath) - 1);
                cachedRunTargetPath[sizeof(cachedRunTargetPath) - 1] = '\0';
            }
        }
    }

    fclose(file);
    return (cachedWorkspacePath[0] != '\0') ? cachedWorkspacePath : NULL;
}

void saveWorkspacePreference(const char* path) {
    if (!path || path[0] == '\0') {
        return;
    }

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

    fprintf(file, "workspace=%s\n", path);
    if (cachedRunTargetPath[0]) {
        fprintf(file, "run_target=%s\n", cachedRunTargetPath);
    }
    fclose(file);

    strncpy(cachedWorkspacePath, path, sizeof(cachedWorkspacePath) - 1);
    cachedWorkspacePath[sizeof(cachedWorkspacePath) - 1] = '\0';
}

const char* loadRunTargetPreference(void) {
    if (cachedRunTargetPath[0] != '\0') {
        return cachedRunTargetPath;
    }

    char configPath[PATH_MAX];
    if (!buildConfigPath(configPath, sizeof(configPath), 1)) {
        return NULL;
    }

    FILE* file = fopen(configPath, "r");
    if (!file) {
        return NULL;
    }

    char line[PATH_MAX + 32];
    const char* prefix = "run_target=";
    size_t prefixLen = strlen(prefix);

    while (fgets(line, sizeof(line), file)) {
        trimTrailingWhitespace(line);
        if (strncmp(line, prefix, prefixLen) == 0) {
            const char* value = line + prefixLen;
            if (*value != '\0') {
                strncpy(cachedRunTargetPath, value, sizeof(cachedRunTargetPath) - 1);
                cachedRunTargetPath[sizeof(cachedRunTargetPath) - 1] = '\0';
            }
        }
    }

    fclose(file);
    return (cachedRunTargetPath[0] != '\0') ? cachedRunTargetPath : NULL;
}

void saveRunTargetPreference(const char* path) {
    if (path && *path) {
        strncpy(cachedRunTargetPath, path, sizeof(cachedRunTargetPath) - 1);
        cachedRunTargetPath[sizeof(cachedRunTargetPath) - 1] = '\0';
    } else {
        cachedRunTargetPath[0] = '\0';
    }

    if (!cachedWorkspacePath[0]) {
        const char* workspace = getWorkspacePath();
        if (workspace && workspace[0]) {
            strncpy(cachedWorkspacePath, workspace, sizeof(cachedWorkspacePath) - 1);
            cachedWorkspacePath[sizeof(cachedWorkspacePath) - 1] = '\0';
        }
    }

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

    fprintf(file, "workspace=%s\n", cachedWorkspacePath[0] ? cachedWorkspacePath : "");
    if (cachedRunTargetPath[0]) {
        fprintf(file, "run_target=%s\n", cachedRunTargetPath);
    }

    fclose(file);
}
