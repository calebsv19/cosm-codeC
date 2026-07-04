#ifndef ERRORS_DIAGNOSTIC_DETAIL_H
#define ERRORS_DIAGNOSTIC_DETAIL_H

#include "core/Diagnostics/diagnostics_engine.h"

#include <stdbool.h>

typedef enum {
    ERRORS_DIAGNOSTIC_DETAIL_LINE_TITLE = 0,
    ERRORS_DIAGNOSTIC_DETAIL_LINE_META,
    ERRORS_DIAGNOSTIC_DETAIL_LINE_MESSAGE,
    ERRORS_DIAGNOSTIC_DETAIL_LINE_HINT,
    ERRORS_DIAGNOSTIC_DETAIL_LINE_EXPLANATION,
    ERRORS_DIAGNOSTIC_DETAIL_LINE_UNITS,
    ERRORS_DIAGNOSTIC_DETAIL_LINE_CONTEXT,
    ERRORS_DIAGNOSTIC_DETAIL_LINE_NAV_CONTEXT
} ErrorsDiagnosticDetailLineKind;

typedef struct {
    ErrorsDiagnosticDetailLineKind kind;
    char text[1400];
    bool hasNavigationTarget;
} ErrorsDiagnosticDetailLine;

enum {
    ERRORS_DIAGNOSTIC_DETAIL_MAX_LINES = 24
};

typedef struct {
    int lineCount;
    ErrorsDiagnosticDetailLine lines[ERRORS_DIAGNOSTIC_DETAIL_MAX_LINES];
} ErrorsDiagnosticDetailModel;

bool errors_diagnostic_detail_build(const Diagnostic* diag,
                                    const char* displayPath,
                                    ErrorsDiagnosticDetailModel* outModel);

#endif
