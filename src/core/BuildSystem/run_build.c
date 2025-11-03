#include "run_build.h"
#include "ide/Panes/Terminal/terminal.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <limits.h>

void runExecutableAndStreamOutput(const char* executablePath) {
    clearTerminal();
    
    if (!executablePath || *executablePath == '\0') {
        printToTerminal("[RunSystem] No executable selected.\n");
        return;
    }

    struct stat st;
    if (stat(executablePath, &st) != 0) {
        char msg[256];
        snprintf(msg, sizeof(msg), "[RunSystem] Unable to access executable: %s (%s)\n",
                 executablePath, strerror(errno));
        printToTerminal(msg);
        return;
    }

    if (!S_ISREG(st.st_mode)) {
        printToTerminal("[RunSystem] Selected path is not a regular file.\n");
        return;
    }

    char dirPath[PATH_MAX];
    strncpy(dirPath, executablePath, sizeof(dirPath) - 1);
    dirPath[sizeof(dirPath) - 1] = '\0';

    char* lastSlash = strrchr(dirPath, '/');
    if (lastSlash) {
        *lastSlash = '\0';
    } else {
        strncpy(dirPath, ".", sizeof(dirPath) - 1);
        dirPath[sizeof(dirPath) - 1] = '\0';
    }

    char infoMsg[512];
    snprintf(infoMsg, sizeof(infoMsg), "[RunSystem] Target: %s\n", executablePath);
    printToTerminal(infoMsg);

    char command[1024];
    snprintf(command, sizeof(command), "cd \"%s\" && \"%s\" 2>&1", dirPath, executablePath);  // redirect stderr to stdout

    FILE* pipe = popen(command, "r");
    if (!pipe) {
        printToTerminal("[RunSystem] Failed to run executable.\n");
        return;
    }

    printToTerminal("[RunSystem] Running executable...\n");

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        printToTerminal(buffer);
    }

    int result = pclose(pipe);
    if (result == 0) {
        printToTerminal("[RunSystem] Executable finished successfully.\n");
    } else {
        printToTerminal("[RunSystem] Executable exited with errors.\n");
    }
}
