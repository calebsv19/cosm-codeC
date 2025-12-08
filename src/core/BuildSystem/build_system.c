#include "build_system.h"
#include "ide/Panes/Terminal/terminal.h"
#include "app/GlobalInfo/project.h"
#include "ide/Panes/ToolPanels/BuildOutput/build_output_panel_state.h"
#include "app/GlobalInfo/core_state.h"
#include "app/GlobalInfo/workspace_prefs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <dirent.h>
#include <time.h>
#include <limits.h>

static BuildStatus currentStatus = BUILD_STATUS_IDLE;
static char buildLog[8192];  // Optional buffer for saving logs
static char lastExecutablePath[1024];

extern char projectPath[1024];        // points to src/Project
extern char projectRootPath[1024]; 

void initBuildSystem() {
    currentStatus = BUILD_STATUS_IDLE;
    buildLog[0] = '\0';
    lastExecutablePath[0] = '\0';
}

void clearBuildOutput() {
    buildLog[0] = '\0';
}

static void searchNewestExecutable(const char* dir,
                                   char* bestPath,
                                   size_t bestPathSize,
                                   time_t* newestTime) {
    DIR* dirp = opendir(dir);
    if (!dirp) return;

    struct dirent* entry;
    char candidate[PATH_MAX];

    while ((entry = readdir(dirp)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        snprintf(candidate, sizeof(candidate), "%s/%s", dir, entry->d_name);

        struct stat st = {0};
        if (stat(candidate, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            searchNewestExecutable(candidate, bestPath, bestPathSize, newestTime);
        } else if (S_ISREG(st.st_mode)) {
            if (!(st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))) continue;
            if (st.st_mtime >= *newestTime) {
                *newestTime = st.st_mtime;
                strncpy(bestPath, candidate, bestPathSize - 1);
                bestPath[bestPathSize - 1] = '\0';
            }
        }
    }

    closedir(dirp);
}

static void updateLastExecutableFromDirectory(const char* outputDir) {
    lastExecutablePath[0] = '\0';
    if (!outputDir || outputDir[0] == '\0') return;

    time_t newestTime = 0;
    char best[PATH_MAX] = {0};
    searchNewestExecutable(outputDir, best, sizeof(best), &newestTime);

    if (best[0] != '\0') {
        snprintf(lastExecutablePath, sizeof(lastExecutablePath), "%s", best);
        if (lastExecutablePath[0] != '\0') {
            const char* existingRunTarget = getRunTargetPath();
            if (!existingRunTarget || existingRunTarget[0] == '\0') {
                setRunTargetPath(lastExecutablePath);
                saveRunTargetPreference(lastExecutablePath);
            }
        }
    }
}

static void expandPathRelative(const char* baseDir,
                               const char* path,
                               char* out,
                               size_t outSize) {
    if (!out || outSize == 0) return;
    out[0] = '\0';

    if (!path || !*path) {
        return;
    }

    if (path[0] == '/') {
        strncpy(out, path, outSize - 1);
        out[outSize - 1] = '\0';
        return;
    }

    if (path[0] == '~') {
        const char* home = getenv("HOME");
        if (!home) home = "";
        if (path[1] == '/' || path[1] == '\\') {
            snprintf(out, outSize, "%s/%s", home, path + 2);
        } else if (path[1] == '\0') {
            snprintf(out, outSize, "%s", home);
        } else {
            snprintf(out, outSize, "%s/%s", home, path + 1);
        }
        return;
    }

    if (baseDir && *baseDir) {
        snprintf(out, outSize, "%s/%s", baseDir, path);
    } else {
        strncpy(out, path, outSize - 1);
        out[outSize - 1] = '\0';
    }
}


void triggerBuild(void) {
    clearBuildOutput();
    if (terminal_activate_task(true, false)) {
        clearTerminal();
    }
    printToTerminal("[BuildSystem] Starting build...\n");
    currentStatus = BUILD_STATUS_RUNNING;
    lastExecutablePath[0] = '\0';

    const WorkspaceBuildConfig* cfg = getWorkspaceBuildConfig();
    const char* customCommand = (cfg && cfg->build_command[0]) ? cfg->build_command : NULL;

    char resolvedWorkingDir[PATH_MAX];
    const char* workingDir = projectPath;
    if (customCommand && cfg->build_working_dir[0]) {
        expandPathRelative(projectPath, cfg->build_working_dir, resolvedWorkingDir, sizeof(resolvedWorkingDir));
        if (resolvedWorkingDir[0]) {
            workingDir = resolvedWorkingDir;
        }
    }

    char resolvedOutputDir[PATH_MAX] = {0};
    const char* outputDirForArtifacts = NULL;

    char command[2048];
    command[0] = '\0';

    if (!customCommand) {
        snprintf(command, sizeof(command),
                 "cd \"%s\" && make 2>&1",
                 projectPath);

        snprintf(resolvedOutputDir, sizeof(resolvedOutputDir), "%s/build", projectPath);
        outputDirForArtifacts = resolvedOutputDir;

        char msg[512];
        snprintf(msg, sizeof(msg), "[BuildSystem] Default build command: make\n");
        printToTerminal(msg);
        snprintf(msg, sizeof(msg), "[BuildSystem] Scanning for artifacts in: %s\n", resolvedOutputDir);
        printToTerminal(msg);
    } else {
        char buildArgs[512];
        buildArgs[0] = '\0';
        if (cfg->build_args[0]) {
            snprintf(buildArgs, sizeof(buildArgs), " %s", cfg->build_args);
        }

        snprintf(command, sizeof(command), "cd \"%s\" && %s%s 2>&1",
                 workingDir, customCommand, buildArgs);

        if (cfg->build_output_dir[0]) {
            expandPathRelative(workingDir, cfg->build_output_dir, resolvedOutputDir, sizeof(resolvedOutputDir));
            if (resolvedOutputDir[0]) {
                outputDirForArtifacts = resolvedOutputDir;
            }
        }

        char summary[512];
        snprintf(summary, sizeof(summary), "[BuildSystem] Working directory: %s\n", workingDir);
        printToTerminal(summary);
        snprintf(summary, sizeof(summary), "[BuildSystem] Command: %s%s\n", customCommand,
                 cfg->build_args[0] ? buildArgs : "");
        printToTerminal(summary);
        if (outputDirForArtifacts) {
            snprintf(summary, sizeof(summary), "[BuildSystem] Artifact directory: %s\n", outputDirForArtifacts);
            printToTerminal(summary);
        }
    }

    FILE* pipe = popen(command, "r");
    if (!pipe) {
        const char* errorMsg = "[BuildSystem] Failed to open build pipe.\n";
        printToTerminal(errorMsg);
        strncat(buildLog, errorMsg, sizeof(buildLog) - strlen(buildLog) - 1);
        currentStatus = BUILD_STATUS_FAILED;
        return;
    }

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        printToTerminal(buffer);
        strncat(buildLog, buffer, sizeof(buildLog) - strlen(buildLog) - 1);
    }

    int result = pclose(pipe);
    if (result == 0) {
        printToTerminal("[BuildSystem] Build completed successfully.\n");
        if (outputDirForArtifacts && outputDirForArtifacts[0]) {
            updateLastExecutableFromDirectory(outputDirForArtifacts);
        }
        pendingProjectRefresh = true;
        currentStatus = BUILD_STATUS_SUCCESS;
    } else {
        printToTerminal("[BuildSystem] Build failed.\n");
        currentStatus = BUILD_STATUS_FAILED;
    }
}


BuildStatus getBuildStatus() {
    return currentStatus;
}

const char* getBuildOutput() {
    return buildLog;
}

const char* getLastBuiltExecutablePath(void) {
    return (lastExecutablePath[0] != '\0') ? lastExecutablePath : NULL;
}

void updateBuildSystem() {
    // Placeholder for future async build thread / subprocess management
}
