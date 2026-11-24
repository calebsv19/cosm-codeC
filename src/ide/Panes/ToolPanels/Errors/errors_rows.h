#ifndef IDE_ERRORS_ROWS_H
#define IDE_ERRORS_ROWS_H

#include "core/Diagnostics/diagnostics_engine.h"

typedef struct {
    char* path;
    Diagnostic* diags;
    int count;
    bool seen;
} ErrorFileSnapshot;

typedef struct {
    const Diagnostic* diag;
    const char* path;
    int fileIndex;
    bool isHeader;
} FlatDiagRef;

typedef struct {
    const ErrorFileSnapshot* files;
    int fileCount;
    const bool* collapsed;
    size_t collapseCount;
    const char* query;
    bool showAll;
    bool showErrors;
    bool showWarnings;
} ErrorsRowSource;

/* Rows borrow diagnostics and paths from the source snapshot. */
int errors_build_rows(const ErrorsRowSource* source, FlatDiagRef* out, int max);

#endif
