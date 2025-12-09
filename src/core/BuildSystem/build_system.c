#include "build_system.h"
#include "ide/Panes/Terminal/terminal.h"
#include "app/GlobalInfo/project.h"
#include "ide/Panes/ToolPanels/BuildOutput/build_output_panel_state.h"
#include "app/GlobalInfo/core_state.h"
#include "app/GlobalInfo/workspace_prefs.h"
#include "core/BuildSystem/build_diagnostics.h"

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

static bool has_makefile(const char* dir) {
    if (!dir || !*dir) return false;
    char path[PATH_MAX];
    struct stat st;
    snprintf(path, sizeof(path), "%s/Makefile", dir);
    if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) return true;
    snprintf(path, sizeof(path), "%s/makefile", dir);
    if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) return true;
    return false;
}

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

// Try to find a runnable artifact in common locations:
// 1) A preferred path, if provided.
// 2) The build output dir (if provided).
// 3) The workspace root as a fallback.
static void discoverRunTarget(const char* preferredPath,
                              const char* outputDir,
                              const char* workspaceRoot) {
    const char* existing = getRunTargetPath();
    if (existing && existing[0]) return;

    if (preferredPath && preferredPath[0]) {
        struct stat st;
        if (stat(preferredPath, &st) == 0 && S_ISREG(st.st_mode) && (st.st_mode & S_IXUSR)) {
            setRunTargetPath(preferredPath);
            saveRunTargetPreference(preferredPath);
            return;
        }
    }

    if (outputDir && outputDir[0]) {
        updateLastExecutableFromDirectory(outputDir);
        if (getRunTargetPath() && getRunTargetPath()[0]) return;
    }

    if (workspaceRoot && workspaceRoot[0]) {
        updateLastExecutableFromDirectory(workspaceRoot);
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
    build_diagnostics_clear();
    setSelectedBuildDiag(-1);

    bool buildTerminalReady = terminal_activate_task(true, false);
    if (buildTerminalReady) {
        clearTerminal();
        terminal_set_follow_output(true);
    } else {
        printToTerminal("[BuildSystem] Warning: Build terminal not found; using current terminal.\n");
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

    char commandForShell[1024] = {0};   // Command to feed into an interactive shell
    char commandForPopen[2048] = {0};   // Command to execute via popen fallback

    bool usePopenFallback = false;

    if (!customCommand) {
        bool useMake = has_makefile(projectPath);
        if (useMake) {
            snprintf(commandForShell, sizeof(commandForShell), "make");
            snprintf(commandForPopen, sizeof(commandForPopen),
                     "cd \"%s\" && make 2>&1",
                     projectPath);

            snprintf(resolvedOutputDir, sizeof(resolvedOutputDir), "%s/build", projectPath);
            outputDirForArtifacts = resolvedOutputDir;

            char msg[512];
            snprintf(msg, sizeof(msg), "[BuildSystem] Default build command: make (Makefile detected)\n");
            printToTerminal(msg);
            snprintf(msg, sizeof(msg), "[BuildSystem] Working directory: %s\n", projectPath);
            printToTerminal(msg);
            snprintf(msg, sizeof(msg), "[BuildSystem] Scanning for artifacts in: %s\n", resolvedOutputDir);
            printToTerminal(msg);
        } else {
            // Fallback: compile all .c files in workspace into build/app
            snprintf(resolvedOutputDir, sizeof(resolvedOutputDir), "%s/build", projectPath);
            outputDirForArtifacts = resolvedOutputDir;
            const char* fallbackCmd =
                "set -e; "
                "mkdir -p build; "
                "find . -name '*.c' ! -path './build/*' -print0 | "
                "xargs -0 cc -std=c11 -Wall -Wextra -g -o build/app && "
                "echo 'Built build/app'";
            snprintf(commandForShell, sizeof(commandForShell), "%s", fallbackCmd);
            snprintf(commandForPopen, sizeof(commandForPopen),
                     "cd \"%s\" && ( %s ) 2>&1",
                     projectPath, fallbackCmd);
            usePopenFallback = true; // ensure we capture output for diagnostics
            char msg[512];
            snprintf(msg, sizeof(msg), "[BuildSystem] No Makefile; compiling all .c files into build/app\n");
            printToTerminal(msg);
            snprintf(msg, sizeof(msg), "[BuildSystem] Working directory: %s\n", projectPath);
            printToTerminal(msg);
            snprintf(msg, sizeof(msg), "[BuildSystem] Command: %s\n", fallbackCmd);
            printToTerminal(msg);
        }
    } else {
        char buildArgs[512];
        buildArgs[0] = '\0';
        if (cfg->build_args[0]) {
            snprintf(buildArgs, sizeof(buildArgs), " %s", cfg->build_args);
        }

        snprintf(commandForShell, sizeof(commandForShell), "%s%s",
                 customCommand, buildArgs);
        snprintf(commandForPopen, sizeof(commandForPopen), "cd \"%s\" && %s%s 2>&1",
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

    // Preferred path: run inside a PTY shell so output streams live into the Build tab.
    if (buildTerminalReady && !usePopenFallback) {
        // Start a fresh shell in the desired working directory.
        if (!terminal_spawn_shell(workingDir, 0, 0)) {
            printToTerminal("[BuildSystem] Failed to start build shell.\n");
            currentStatus = BUILD_STATUS_FAILED;
            return;
        }

        char dispatch[512];
        snprintf(dispatch, sizeof(dispatch), "[BuildSystem] Executing in shell: %s\n", commandForShell);
        printToTerminal(dispatch);

        // Send the command to the shell.
        if (commandForShell[0]) {
            terminal_send_text(commandForShell, strlen(commandForShell));
            terminal_send_text("\n", 1);
        }

        // We cannot synchronously know success here; leave status as running and let user read output.
        return;
    }

    // Fallback: run synchronously with popen in the current terminal (also used for no-Makefile path).
    FILE* pipe = popen(commandForPopen, "r");
    if (!pipe) {
        const char* errorMsg = "[BuildSystem] Failed to open build pipe.\n";
        printToTerminal(errorMsg);
        strncat(buildLog, errorMsg, sizeof(buildLog) - strlen(buildLog) - 1);
        currentStatus = BUILD_STATUS_FAILED;
        return;
    }
    char startLine[256];
    snprintf(startLine, sizeof(startLine), "[BuildSystem] Executing: %s\n", commandForPopen);
    printToTerminal(startLine);

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        printToTerminal(buffer);
        build_diagnostics_feed_chunk(buffer, strlen(buffer));
        strncat(buildLog, buffer, sizeof(buildLog) - strlen(buildLog) - 1);
    }

    int result = pclose(pipe);
    if (result == 0) {
        printToTerminal("[BuildSystem] Build completed successfully.\n");
        const char* workspace = getWorkspacePath();
        discoverRunTarget(lastExecutablePath, outputDirForArtifacts, workspace);
        pendingProjectRefresh = true;
        currentStatus = BUILD_STATUS_SUCCESS;
    } else {
        printToTerminal("[BuildSystem] Build failed.\n");
        currentStatus = BUILD_STATUS_FAILED;
    }
    build_diagnostics_save(projectPath);
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
