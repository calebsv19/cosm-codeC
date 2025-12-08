#include "run_build.h"
#include "ide/Panes/Terminal/terminal.h"
#include "app/GlobalInfo/core_state.h"
#include "app/GlobalInfo/workspace_prefs.h"
#include "ide/Panes/Terminal/terminal.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <limits.h>

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

static void replaceToken(const char* src,
                         const char* token,
                         const char* replacement,
                         char* dst,
                         size_t dstSize) {
    if (!src || !token || !dst || dstSize == 0) return;
    size_t tokenLen = strlen(token);
    size_t repLen = replacement ? strlen(replacement) : 0;
    size_t written = 0;

    while (*src && written + 1 < dstSize) {
        if (tokenLen > 0 && strncmp(src, token, tokenLen) == 0) {
            if (repLen > 0) {
                size_t copy = repLen;
                if (written + copy >= dstSize) {
                    copy = dstSize - written - 1;
                }
                memcpy(dst + written, replacement, copy);
                written += copy;
            }
            src += tokenLen;
        } else {
            dst[written++] = *src++;
        }
    }

    dst[written] = '\0';
}

void runExecutableAndStreamOutput(const char* executablePath) {
    // Route output to the Run terminal session
    if (terminal_activate_task(false, true)) {
        clearTerminal();
    }

    const WorkspaceBuildConfig* cfg = getWorkspaceBuildConfig();
    bool useCustomCommand = cfg && cfg->run_command[0];

    if (!useCustomCommand) {
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

        const char* workspace = getWorkspacePath();
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

        const char* workingDir = (workspace && *workspace) ? workspace : dirPath;
        char infoMsg[512];
        snprintf(infoMsg, sizeof(infoMsg), "[RunSystem] Target: %s\n", executablePath);
        printToTerminal(infoMsg);
        if (workingDir && *workingDir) {
            snprintf(infoMsg, sizeof(infoMsg), "[RunSystem] Working directory: %s\n", workingDir);
            printToTerminal(infoMsg);
        }

        char command[1024];
        snprintf(command, sizeof(command), "cd \"%s\" && \"%s\" 2>&1", workingDir, executablePath);

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
        return;
    }

    const char* workspace = getWorkspacePath();
    if (!workspace || !*workspace) workspace = ".";

    char resolvedWorkingDir[PATH_MAX];
    resolvedWorkingDir[0] = '\0';
    if (cfg->run_working_dir[0]) {
        expandPathRelative(workspace, cfg->run_working_dir, resolvedWorkingDir, sizeof(resolvedWorkingDir));
    }
    if (!resolvedWorkingDir[0]) {
        strncpy(resolvedWorkingDir, workspace, sizeof(resolvedWorkingDir) - 1);
        resolvedWorkingDir[sizeof(resolvedWorkingDir) - 1] = '\0';
    }

    char expandedArgs[1024] = {0};
    bool argsContainTarget = (cfg->run_args[0] && strstr(cfg->run_args, "{TARGET}"));
    if (cfg->run_args[0]) {
        replaceToken(cfg->run_args, "{TARGET}", executablePath ? executablePath : "", expandedArgs, sizeof(expandedArgs));
    }

    if (!argsContainTarget && executablePath && *executablePath) {
        size_t len = strlen(expandedArgs);
        if (len > 0) {
            strncat(expandedArgs, " ", sizeof(expandedArgs) - len - 1);
            len = strlen(expandedArgs);
        }
        strncat(expandedArgs, "\"", sizeof(expandedArgs) - len - 1);
        strncat(expandedArgs, executablePath, sizeof(expandedArgs) - strlen(expandedArgs) - 1);
        strncat(expandedArgs, "\"", sizeof(expandedArgs) - strlen(expandedArgs) - 1);
    }

    char command[2048];
    if (expandedArgs[0]) {
        snprintf(command, sizeof(command), "cd \"%s\" && %s %s 2>&1",
                 resolvedWorkingDir, cfg->run_command, expandedArgs);
    } else {
        snprintf(command, sizeof(command), "cd \"%s\" && %s 2>&1",
                 resolvedWorkingDir, cfg->run_command);
    }

    char infoMsg[512];
    snprintf(infoMsg, sizeof(infoMsg), "[RunSystem] Working directory: %s\n", resolvedWorkingDir);
    printToTerminal(infoMsg);
    snprintf(infoMsg, sizeof(infoMsg), "[RunSystem] Command: %s%s%s\n",
             cfg->run_command,
             expandedArgs[0] ? " " : "",
             expandedArgs);
    printToTerminal(infoMsg);

    FILE* pipe = popen(command, "r");
    if (!pipe) {
        printToTerminal("[RunSystem] Failed to run custom command.\n");
        return;
    }

    printToTerminal("[RunSystem] Running command...\n");

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        printToTerminal(buffer);
    }

    int result = pclose(pipe);
    if (result == 0) {
        printToTerminal("[RunSystem] Command finished successfully.\n");
    } else {
        printToTerminal("[RunSystem] Command exited with errors.\n");
    }
}
