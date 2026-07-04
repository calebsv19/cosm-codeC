#ifndef IDE_GIT_COMMAND_RUNNER_H
#define IDE_GIT_COMMAND_RUNNER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/types.h>

typedef enum {
    GIT_COMMAND_STDERR_INHERIT = 0,
    GIT_COMMAND_STDERR_TO_STDOUT,
    GIT_COMMAND_STDERR_TO_NULL
} GitCommandStderrMode;

typedef struct {
    FILE* stream;
    pid_t pid;
} GitCommandProcess;

bool git_command_process_start(const char* workdir,
                               const char* const argv[],
                               GitCommandStderrMode stderr_mode,
                               GitCommandProcess* out_process);

int git_command_process_close(GitCommandProcess* process);

bool git_command_capture_first_line(const char* workdir,
                                    const char* const argv[],
                                    GitCommandStderrMode stderr_mode,
                                    char* out_line,
                                    size_t out_line_cap);

#endif
