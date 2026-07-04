#include "ide/Panes/ToolPanels/Git/git_command_runner.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static bool file_exists(const char* path) {
    struct stat st;
    return path && stat(path, &st) == 0;
}

static bool make_shell_sensitive_workdir(char* out_root,
                                         size_t root_cap,
                                         char* out_workdir,
                                         size_t workdir_cap,
                                         char* out_marker,
                                         size_t marker_cap) {
    if (!out_root || !out_workdir || !out_marker) return false;
    snprintf(out_root, root_cap, "%s", "/tmp/ide_git_runner_test.XXXXXX");
    if (!mkdtemp(out_root)) {
        return false;
    }

    snprintf(out_workdir,
             workdir_cap,
             "%s/repo\"; touch marker_from_shell; printf \"",
             out_root);
    if (mkdir(out_workdir, 0700) != 0) {
        return false;
    }

    snprintf(out_marker, marker_cap, "%s/marker_from_shell", out_root);
    return true;
}

int main(void) {
    char root[512] = {0};
    char workdir[768] = {0};
    char marker[768] = {0};
    if (!make_shell_sensitive_workdir(root, sizeof(root), workdir, sizeof(workdir), marker, sizeof(marker))) {
        fprintf(stderr, "failed to create shell-sensitive workdir fixture\n");
        return 1;
    }

    char line[1024] = {0};
    const char* const pwd_argv[] = {"/bin/pwd", NULL};
    if (!git_command_capture_first_line(workdir,
                                        pwd_argv,
                                        GIT_COMMAND_STDERR_TO_NULL,
                                        line,
                                        sizeof(line))) {
        fprintf(stderr, "runner failed to execute argv command in fixture dir\n");
        return 1;
    }

    char expected_path[1024] = {0};
    char actual_path[1024] = {0};
    if (!realpath(workdir, expected_path) || !realpath(line, actual_path)) {
        fprintf(stderr, "failed to resolve runner paths\n");
        return 1;
    }
    if (strcmp(actual_path, expected_path) != 0) {
        fprintf(stderr, "runner did not chdir into fixture dir: %s\n", line);
        return 1;
    }

    if (file_exists(marker)) {
        fprintf(stderr, "shell-sensitive path created marker: %s\n", marker);
        return 1;
    }

    const char* const printf_argv[] = {"/usr/bin/printf", "hello\nsecond\n", NULL};
    if (!git_command_capture_first_line(workdir,
                                        printf_argv,
                                        GIT_COMMAND_STDERR_TO_NULL,
                                        line,
                                        sizeof(line))) {
        fprintf(stderr, "runner failed to capture printf output\n");
        return 1;
    }
    if (strcmp(line, "hello") != 0) {
        fprintf(stderr, "unexpected captured line: %s\n", line);
        return 1;
    }

    return 0;
}
