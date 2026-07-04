#ifndef ERRORS_CONTEXT_DETAIL_H
#define ERRORS_CONTEXT_DETAIL_H

#include "core/Diagnostics/diagnostics_engine.h"
#include <stdbool.h>
#include <stddef.h>

typedef enum {
    ERRORS_CONTEXT_ROW_SUMMARY = 0,
    ERRORS_CONTEXT_ROW_INCLUDE,
    ERRORS_CONTEXT_ROW_MACRO,
    ERRORS_CONTEXT_ROW_DETAILS
} ErrorsContextRowKind;

typedef struct {
    ErrorsContextRowKind kind;
    char text[1024];
    bool hasNavigationTarget;
    char targetPath[1024];
    int targetLine;
    int targetColumn;
} ErrorsContextDetailRow;

enum { ERRORS_CONTEXT_DETAIL_MAX_ROWS = 10 };

typedef struct {
    int includeCount;
    int macroCount;
    bool hasDetails;
    bool hasNavigationTarget;
    char targetPath[1024];
    int targetLine;
    int targetColumn;
    char targetKind[16];
    int rowCount;
    ErrorsContextDetailRow rows[ERRORS_CONTEXT_DETAIL_MAX_ROWS];
} ErrorsContextDetail;

bool errors_context_detail_for_diagnostic(const Diagnostic* diag,
                                          ErrorsContextDetail* outDetail);

#endif
