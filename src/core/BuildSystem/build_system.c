#include "build_system.h"
#include "ide/Panes/Terminal/terminal.h"
#include "app/GlobalInfo/project.h"
#include "ide/Panes/ToolPanels/BuildOutput/build_output_panel_state.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>  // mkdir
#include <sys/types.h>

static BuildStatus currentStatus = BUILD_STATUS_IDLE;
static char buildLog[8192];  // Optional buffer for saving logs

extern char projectPath[1024];        // points to src/Project
extern char projectRootPath[1024]; 

void initBuildSystem() {
    currentStatus = BUILD_STATUS_IDLE;
    buildLog[0] = '\0';
}

void clearBuildOutput() {
    buildLog[0] = '\0';
}


void triggerBuild(void) {
    clearBuildOutput();
    clearTerminal();
    printToTerminal("[BuildSystem] Starting build...\n");
    currentStatus = BUILD_STATUS_RUNNING;

    BuildOutputPanelState* state = getBuildOutputPanelState();
    const char* outputFolder = "last_build";  // fallback

    if (state->selectedBuildDirectory && state->selectedBuildDirectory->fullPath) {
        outputFolder = state->selectedBuildDirectory->label;
    }

    char fullOutputPath[1024];
    snprintf(fullOutputPath, sizeof(fullOutputPath), "%s/BuildOutputs/%s", projectRootPath, outputFolder);

    // Ensure output directory exists
    struct stat st = {0};
    if (stat(fullOutputPath, &st) == -1) {
        if (mkdir(fullOutputPath, 0755) != 0) {
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
             "cd %s && make PROJECT_ROOT=%s OUTPUT_DIR=%s 2>&1",
             projectPath, projectRootPath, fullOutputPath);

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

void updateBuildSystem() {
    // Placeholder for future async build thread / subprocess management
}

