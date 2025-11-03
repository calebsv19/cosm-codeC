#ifndef BUILD_SYSTEM_H
#define BUILD_SYSTEM_H

#include <stdbool.h>

typedef enum {
    BUILD_STATUS_IDLE,
    BUILD_STATUS_RUNNING,
    BUILD_STATUS_SUCCESS,
    BUILD_STATUS_FAILED
} BuildStatus;

void initBuildSystem();
void triggerBuild();
void updateBuildSystem();  // For async polling, if needed
BuildStatus getBuildStatus();
const char* getBuildOutput();  // Log output from build
void clearBuildOutput();
const char* getLastBuiltExecutablePath(void);

#endif
