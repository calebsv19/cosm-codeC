#ifndef IDE_OWNED_PROCESS_H
#define IDE_OWNED_PROCESS_H
#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>
/* Main-owner synchronous stream. EOF and child exit are separate states.
 * Call read_line, not stdio reads directly; close always reaps the direct child.
 * Closing before output EOF cancels the owned group; drained commands retain
 * their ordinary exit status. */
typedef struct {
    FILE* stream;
    pid_t pid;
    bool reaped, eof;
    int status;
} OwnedProcess;
bool owned_process_start(const char* workdir, const char* const argv[], int stderr_mode, OwnedProcess* out);
bool owned_process_shell(const char* command, OwnedProcess* out);
char* owned_process_read_line(OwnedProcess* process, char* line, size_t capacity);
int owned_process_close(OwnedProcess* process);
#endif
