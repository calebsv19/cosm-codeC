#ifndef DIAGNOSTIC_CONTEXT_H
#define DIAGNOSTIC_CONTEXT_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    char* file;
    int line;
    int column;
    int length;
    int codeId;
    char* codeName;
    char* message;
    char* includeStackJson;
    char* macroTraceJson;
    char* detailsJson;
} DiagnosticContextRecord;

void diagnostic_context_clear(void);
bool diagnostic_context_load_json_text(const char* text);
bool diagnostic_context_save(const char* workspaceRoot);
bool diagnostic_context_load(const char* workspaceRoot);
size_t diagnostic_context_count(void);
const DiagnosticContextRecord* diagnostic_context_at(size_t index);
const DiagnosticContextRecord* diagnostic_context_find(const char* file,
                                                       int line,
                                                       int column,
                                                       int codeId,
                                                       const char* codeName,
                                                       const char* message);

#endif
