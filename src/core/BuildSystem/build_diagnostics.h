#ifndef BUILD_DIAGNOSTICS_H
#define BUILD_DIAGNOSTICS_H

#include <stddef.h>
#include <stdbool.h>

#define BUILD_DIAG_PATH_MAX 512
#define BUILD_DIAG_MSG_MAX 512
#define BUILD_DIAG_NOTES_MAX 1024

typedef struct {
    char path[BUILD_DIAG_PATH_MAX];
    int line;
    int col;
    bool isError;
    char message[BUILD_DIAG_MSG_MAX];
    char notes[BUILD_DIAG_NOTES_MAX];
} BuildDiagnostic;

void build_diagnostics_clear(void);
void build_diagnostics_feed_chunk(const char* data, size_t len);
const BuildDiagnostic* build_diagnostics_get(size_t* count);
void build_diagnostics_save(const char* workspaceRoot);
void build_diagnostics_load(const char* workspaceRoot);

#endif
