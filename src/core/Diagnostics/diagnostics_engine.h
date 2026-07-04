#ifndef DIAGNOSTICS_ENGINE_H
#define DIAGNOSTICS_ENGINE_H

#include <stdbool.h>
#include <stddef.h>

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

typedef enum {
    DIAGNOSTIC_IO_NONE = 0,
    DIAGNOSTIC_IO_SAVE,
    DIAGNOSTIC_IO_LOAD
} DiagnosticIoOperation;

typedef enum {
    DIAGNOSTIC_IO_REASON_NONE = 0,
    DIAGNOSTIC_IO_REASON_INVALID_WORKSPACE,
    DIAGNOSTIC_IO_REASON_MISSING_OK,
    DIAGNOSTIC_IO_REASON_SAVED,
    DIAGNOSTIC_IO_REASON_LOADED,
    DIAGNOSTIC_IO_REASON_SAVE_MKDIR_FAILED,
    DIAGNOSTIC_IO_REASON_SAVE_SERIALIZE_FAILED,
    DIAGNOSTIC_IO_REASON_SAVE_OPEN_FAILED,
    DIAGNOSTIC_IO_REASON_SAVE_WRITE_FAILED,
    DIAGNOSTIC_IO_REASON_LOAD_OPEN_MISSING,
    DIAGNOSTIC_IO_REASON_LOAD_STAT_FAILED,
    DIAGNOSTIC_IO_REASON_LOAD_EMPTY,
    DIAGNOSTIC_IO_REASON_LOAD_OVERSIZED,
    DIAGNOSTIC_IO_REASON_LOAD_READ_FAILED,
    DIAGNOSTIC_IO_REASON_LOAD_ALLOC_FAILED,
    DIAGNOSTIC_IO_REASON_LOAD_INVALID_JSON,
    DIAGNOSTIC_IO_REASON_LOAD_INVALID_ROOT,
    DIAGNOSTIC_IO_REASON_LOAD_MALFORMED_ROWS
} DiagnosticIoReason;

typedef struct {
    DiagnosticIoOperation operation;
    DiagnosticIoReason reason;
    bool ok;
    bool noisy_failure;
    char path[1024];
    int saved_rows;
    int loaded_rows;
    int malformed_rows;
    long size_bytes;
} DiagnosticIoReport;

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
bool diagnostics_save_report(const char* workspaceRoot, DiagnosticIoReport* outReport);
bool diagnostics_load_report(const char* workspaceRoot, DiagnosticIoReport* outReport);
void diagnostics_last_io_report(DiagnosticIoReport* outReport);
const char* diagnostics_io_reason_string(DiagnosticIoReason reason);

#endif
