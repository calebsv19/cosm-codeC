#include "ide/Panes/ToolPanels/Git/git_command_runner.h"

#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

bool git_command_process_start(const char* dir,const char* const argv[],
                               GitCommandStderrMode mode,GitCommandProcess* out) {
    return owned_process_start(dir,argv,(int)mode,out);
}
int git_command_process_close(GitCommandProcess* p) { return owned_process_close(p); }

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
        if (owned_process_read_line(&process, out_line, out_line_cap)) {
            out_line[strcspn(out_line, "\r\n")] = '\0';
        } else {
            out_line[0] = '\0';
        }
    }

    char sink[256];
    while (owned_process_read_line(&process, sink, sizeof(sink))) {
    }

    int rc = git_command_process_close(&process);
    return rc == 0;
}
