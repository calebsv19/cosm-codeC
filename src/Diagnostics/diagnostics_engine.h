#ifndef DIAGNOSTICS_ENGINE_H
#define DIAGNOSTICS_ENGINE_H

#include <stdbool.h>

typedef enum {
    DIAG_SEVERITY_INFO,
    DIAG_SEVERITY_WARNING,
    DIAG_SEVERITY_ERROR
} DiagnosticSeverity;

typedef struct {
    const char* filePath;
    int line;
    int column;
    const char* message;
    DiagnosticSeverity severity;
} Diagnostic;

void initDiagnosticsEngine();
void clearDiagnostics();
void addDiagnostic(const char* file, int line, int col, const char* msg, DiagnosticSeverity severity);
int getDiagnosticCount();
const Diagnostic* getDiagnosticAt(int index);

#endif

