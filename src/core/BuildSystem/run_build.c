#include "run_build.h"
#include "ide/Panes/Terminal/terminal.h"

#include <stdio.h>
#include <stdlib.h>

void runExecutableAndStreamOutput(const char* executablePath) {
    clearTerminal();
    
    char command[1024];
    snprintf(command, sizeof(command), "%s 2>&1", executablePath);  // redirect stderr to stdout

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

