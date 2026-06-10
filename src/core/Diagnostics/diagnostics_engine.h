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
    DIAG_CATEGORY_CODEGEN = 7,
    DIAG_CATEGORY_EXTENSION = 8
} DiagnosticCategory;

typedef struct {
    const char* filePath;
    int line;
    int column;
    int length;
    const char* message;
    const char* hint;
    DiagnosticSeverity severity;
    DiagnosticCategory category;
    int codeId;
    const char* codeName;
    const char* stage;
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
void addDiagnosticWithDetails(const char* file,
                              int line,
                              int col,
                              int length,
                              const char* msg,
                              const char* hint,
                              DiagnosticSeverity severity,
                              DiagnosticCategory category,
                              int codeId,
                              const char* codeName,
                              const char* stage);
int getDiagnosticCount();
const Diagnostic* getDiagnosticAt(int index);
const char* diagnostic_category_name(DiagnosticCategory category);
const char* diagnostic_code_name(int codeId);
const char* diagnostic_stage_name(int codeId);

// Persistence helpers (store under workspace/ide_files/analysis_diagnostics.json)
void diagnostics_save(const char* workspaceRoot);
void diagnostics_load(const char* workspaceRoot);

#endif
