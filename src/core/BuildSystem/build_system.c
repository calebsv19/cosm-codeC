#include "build_system.h"
#include "ide/Panes/Terminal/terminal.h"
#include "app/GlobalInfo/project.h"
#include "ide/Panes/ToolPanels/BuildOutput/build_output_panel_state.h"
#include "app/GlobalInfo/core_state.h"
#include "app/GlobalInfo/workspace_prefs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>  // mkdir
#include <sys/types.h>
#include <errno.h>
#include <dirent.h>
#include <time.h>

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

static void updateLastExecutableFromDirectory(const char* outputDir) {
    lastExecutablePath[0] = '\0';
    if (!outputDir || outputDir[0] == '\0') return;

    DIR* dir = opendir(outputDir);
    if (!dir) return;

    struct dirent* entry;
    time_t newestTime = 0;
    char candidate[1024];
    char best[1024] = {0};

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        snprintf(candidate, sizeof(candidate), "%s/%s", outputDir, entry->d_name);

        struct stat st = {0};
        if (stat(candidate, &st) != 0) continue;
        if (!S_ISREG(st.st_mode)) continue;

        if (st.st_mtime >= newestTime) {
            newestTime = st.st_mtime;
            snprintf(best, sizeof(best), "%s", candidate);
        }
    }

    closedir(dir);

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


void triggerBuild(void) {
    clearBuildOutput();
    clearTerminal();
    printToTerminal("[BuildSystem] Starting build...\n");
    currentStatus = BUILD_STATUS_RUNNING;
    lastExecutablePath[0] = '\0';

    BuildOutputPanelState* state = getBuildOutputPanelState();
    const char* outputFolder = "last_build";  // fallback

    if (state->selectedBuildDirectory && state->selectedBuildDirectory->fullPath) {
        outputFolder = state->selectedBuildDirectory->label;
    }

    char fullOutputPath[1024];
    char buildOutputRoot[1024];

    snprintf(buildOutputRoot, sizeof(buildOutputRoot), "%s/BuildOutputs", projectPath);
    struct stat st = {0};
    if (stat(buildOutputRoot, &st) == -1) {
        if (mkdir(buildOutputRoot, 0755) != 0 && errno != EEXIST) {
            printToTerminal("[BuildSystem] Failed to create BuildOutputs folder.\n");
            currentStatus = BUILD_STATUS_FAILED;
            return;
        }
    }

    snprintf(fullOutputPath, sizeof(fullOutputPath), "%s/%s", buildOutputRoot, outputFolder);

    // Ensure output directory exists
    if (stat(fullOutputPath, &st) == -1) {
        if (mkdir(fullOutputPath, 0755) != 0 && errno != EEXIST) {
            printToTerminal("[BuildSystem] Failed to create build output folder.\n");
            currentStatus = BUILD_STATUS_FAILED;
            return;
        }
    }

    // Log
    char msg[512];
    snprintf(msg, sizeof(msg), "[BuildSystem] Using output path: %s\n", fullOutputPath);
    printToTerminal(msg);

    // Construct build command
    char command[1024];
    snprintf(command, sizeof(command),
             "cd \"%s\" && make PROJECT_ROOT=\"%s\" OUTPUT_DIR=\"%s\" IDE_ROOT=\"%s\" 2>&1",
             projectPath, projectPath, fullOutputPath, projectRootPath);

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
        updateLastExecutableFromDirectory(fullOutputPath);
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
