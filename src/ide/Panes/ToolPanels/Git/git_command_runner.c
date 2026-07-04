#include "ide/Panes/ToolPanels/Git/git_command_runner.h"

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static bool git_command_argv_valid(const char* const argv[]) {
    return argv && argv[0] && argv[0][0];
}

static int git_command_exit_code_from_status(int status) {
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return 1;
}

bool git_command_process_start(const char* workdir,
                               const char* const argv[],
                               GitCommandStderrMode stderr_mode,
                               GitCommandProcess* out_process) {
    if (out_process) {
        out_process->stream = NULL;
        out_process->pid = -1;
    }
    if (!workdir || !workdir[0] || !git_command_argv_valid(argv) || !out_process) {
        return false;
    }

    int fds[2] = {-1, -1};
    if (pipe(fds) != 0) {
        return false;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(fds[0]);
        close(fds[1]);
        return false;
    }

    if (pid == 0) {
        close(fds[0]);
        if (chdir(workdir) != 0) {
            _exit(127);
        }
        if (dup2(fds[1], STDOUT_FILENO) < 0) {
            _exit(127);
        }
        if (stderr_mode == GIT_COMMAND_STDERR_TO_STDOUT) {
            if (dup2(fds[1], STDERR_FILENO) < 0) {
                _exit(127);
            }
        } else if (stderr_mode == GIT_COMMAND_STDERR_TO_NULL) {
            int null_fd = open("/dev/null", O_WRONLY);
            if (null_fd >= 0) {
                (void)dup2(null_fd, STDERR_FILENO);
                close(null_fd);
            }
        }
        close(fds[1]);
        execvp(argv[0], (char* const*)argv);
        _exit(127);
    }

    close(fds[1]);
    FILE* stream = fdopen(fds[0], "r");
    if (!stream) {
        close(fds[0]);
        int status = 0;
        (void)waitpid(pid, &status, 0);
        return false;
    }

    out_process->stream = stream;
    out_process->pid = pid;
    return true;
}

int git_command_process_close(GitCommandProcess* process) {
    if (!process || process->pid <= 0) {
        return 1;
    }

    if (process->stream) {
        fclose(process->stream);
        process->stream = NULL;
    }

    int status = 0;
    pid_t waited = waitpid(process->pid, &status, 0);
    process->pid = -1;
    if (waited < 0) {
        return 1;
    }
    return git_command_exit_code_from_status(status);
}

bool git_command_capture_first_line(const char* workdir,
                                    const char* const argv[],
                                    GitCommandStderrMode stderr_mode,
                                    char* out_line,
                                    size_t out_line_cap) {
    if (out_line && out_line_cap > 0) {
        out_line[0] = '\0';
    }

    GitCommandProcess process = {0};
    if (!git_command_process_start(workdir, argv, stderr_mode, &process)) {
        return false;
    }

    if (out_line && out_line_cap > 0) {
        if (fgets(out_line, (int)out_line_cap, process.stream)) {
            out_line[strcspn(out_line, "\r\n")] = '\0';
        } else {
            out_line[0] = '\0';
        }
    }

    char sink[256];
    while (fgets(sink, sizeof(sink), process.stream)) {
    }

    int rc = git_command_process_close(&process);
    return rc == 0;
}
