#include "diagnostics_engine.h"
#include <stdlib.h>
#include <string.h>

#define MAX_DIAGNOSTICS 512

static Diagnostic diagnostics[MAX_DIAGNOSTICS];
static int diagnosticCount = 0;

void initDiagnosticsEngine() {
    diagnosticCount = 0;
}

void clearDiagnostics() {
    diagnosticCount = 0;
}

void addDiagnostic(const char* file, int line, int col, const char* msg, DiagnosticSeverity severity) {
    if (diagnosticCount >= MAX_DIAGNOSTICS) return;

    diagnostics[diagnosticCount].filePath = strdup(file);
    diagnostics[diagnosticCount].line = line;
    diagnostics[diagnosticCount].column = col;
    diagnostics[diagnosticCount].message = strdup(msg);
    diagnostics[diagnosticCount].severity = severity;

    diagnosticCount++;
}

int getDiagnosticCount() {
    return diagnosticCount;
}

const Diagnostic* getDiagnosticAt(int index) {
    if (index < 0 || index >= diagnosticCount) return NULL;
    return &diagnostics[index];
}

