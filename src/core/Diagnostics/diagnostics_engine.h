#ifndef DIAGNOSTICS_ENGINE_H
#define DIAGNOSTICS_ENGINE_H

#include <stdbool.h>

typedef enum {
    DIAG_SEVERITY_INFO,
    DIAG_SEVERITY_WARNING,
    DIAG_SEVERITY_ERROR
} DiagnosticSeverity;

typedef enum {
    DIAG_CATEGORY_UNKNOWN = 0,
    DIAG_CATEGORY_BUILD = 1,
    DIAG_CATEGORY_ANALYSIS = 2,
    DIAG_CATEGORY_PARSER = 3,
    DIAG_CATEGORY_SEMANTIC = 4,
    DIAG_CATEGORY_PREPROCESSOR = 5,
    DIAG_CATEGORY_LEXER = 6,
    DIAG_CATEGORY_CODEGEN = 7
} DiagnosticCategory;

typedef struct {
    const char* filePath;
    int line;
    int column;
    const char* message;
    DiagnosticSeverity severity;
    DiagnosticCategory category;
    int codeId;
} Diagnostic;

void initDiagnosticsEngine();
void clearDiagnostics();
void addDiagnostic(const char* file, int line, int col, const char* msg, DiagnosticSeverity severity);
void addDiagnosticWithMeta(const char* file,
                           int line,
                           int col,
                           const char* msg,
                           DiagnosticSeverity severity,
                           DiagnosticCategory category,
                           int codeId);
int getDiagnosticCount();
const Diagnostic* getDiagnosticAt(int index);
const char* diagnostic_category_name(DiagnosticCategory category);

// Persistence helpers (store under workspace/ide_files/analysis_diagnostics.json)
void diagnostics_save(const char* workspaceRoot);
void diagnostics_load(const char* workspaceRoot);

#endif
