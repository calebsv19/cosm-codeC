#include "diagnostics_engine.h"
#include <json-c/json.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_DIAGNOSTICS 512

static Diagnostic diagnostics[MAX_DIAGNOSTICS];
static int diagnosticCount = 0;

const char* diagnostic_category_name(DiagnosticCategory category) {
    switch (category) {
        case DIAG_CATEGORY_BUILD: return "build";
        case DIAG_CATEGORY_ANALYSIS: return "analysis";
        case DIAG_CATEGORY_PARSER: return "parser";
        case DIAG_CATEGORY_SEMANTIC: return "semantic";
        case DIAG_CATEGORY_PREPROCESSOR: return "preprocessor";
        case DIAG_CATEGORY_LEXER: return "lexer";
        case DIAG_CATEGORY_CODEGEN: return "codegen";
        case DIAG_CATEGORY_UNKNOWN:
        default: return "unknown";
    }
}

void initDiagnosticsEngine() {
    diagnosticCount = 0;
}

void clearDiagnostics() {
    diagnosticCount = 0;
}

void addDiagnostic(const char* file, int line, int col, const char* msg, DiagnosticSeverity severity) {
    addDiagnosticWithMeta(file, line, col, msg, severity, DIAG_CATEGORY_UNKNOWN, 0);
}

void addDiagnosticWithMeta(const char* file,
                           int line,
                           int col,
                           const char* msg,
                           DiagnosticSeverity severity,
                           DiagnosticCategory category,
                           int codeId) {
    if (diagnosticCount >= MAX_DIAGNOSTICS) return;

    diagnostics[diagnosticCount].filePath = strdup(file);
    diagnostics[diagnosticCount].line = line;
    diagnostics[diagnosticCount].column = col;
    diagnostics[diagnosticCount].message = strdup(msg);
    diagnostics[diagnosticCount].severity = severity;
    diagnostics[diagnosticCount].category = category;
    diagnostics[diagnosticCount].codeId = codeId;

    diagnosticCount++;
}

int getDiagnosticCount() {
    return diagnosticCount;
}

const Diagnostic* getDiagnosticAt(int index) {
    if (index < 0 || index >= diagnosticCount) return NULL;
    return &diagnostics[index];
}

void diagnostics_save(const char* workspaceRoot) {
    if (!workspaceRoot || !*workspaceRoot) return;
    char path[1024];
    snprintf(path, sizeof(path), "%s/ide_files", workspaceRoot);
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        mkdir(path, 0755);
    }
    snprintf(path, sizeof(path), "%s/ide_files/analysis_diagnostics.json", workspaceRoot);

    json_object* arr = json_object_new_array();
    for (int i = 0; i < diagnosticCount; ++i) {
        const Diagnostic* d = &diagnostics[i];
        json_object* obj = json_object_new_object();
        json_object_object_add(obj, "file", json_object_new_string(d->filePath ? d->filePath : ""));
        json_object_object_add(obj, "line", json_object_new_int(d->line));
        json_object_object_add(obj, "col", json_object_new_int(d->column));
        json_object_object_add(obj, "severity", json_object_new_int(d->severity));
        json_object_object_add(obj, "category", json_object_new_int((int)d->category));
        json_object_object_add(obj, "code_id", json_object_new_int(d->codeId));
        json_object_object_add(obj, "message", json_object_new_string(d->message ? d->message : ""));
        json_object_array_add(arr, obj);
    }

    const char* serialized = json_object_to_json_string_ext(arr, JSON_C_TO_STRING_PLAIN);
    FILE* f = fopen(path, "w");
    if (f && serialized) {
        fputs(serialized, f);
        fclose(f);
    } else if (f) {
        fclose(f);
    }
    json_object_put(arr);
}

void diagnostics_load(const char* workspaceRoot) {
    clearDiagnostics();
    if (!workspaceRoot || !*workspaceRoot) return;
    char path[1024];
    snprintf(path, sizeof(path), "%s/ide_files/analysis_diagnostics.json", workspaceRoot);
    FILE* f = fopen(path, "r");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0 || len > 1 << 20) {
        fclose(f);
        return;
    }
    char* buf = malloc((size_t)len + 1);
    if (!buf) {
        fclose(f);
        return;
    }
    fread(buf, 1, (size_t)len, f);
    buf[len] = '\0';
    fclose(f);

    json_object* root = json_tokener_parse(buf);
    free(buf);
    if (!root || !json_object_is_type(root, json_type_array)) {
        if (root) json_object_put(root);
        return;
    }

    size_t arrLen = json_object_array_length(root);
    for (size_t i = 0; i < arrLen && diagnosticCount < MAX_DIAGNOSTICS; ++i) {
        json_object* obj = json_object_array_get_idx(root, i);
        if (!obj) continue;
        json_object* jfile = NULL;
        json_object* jline = NULL;
        json_object* jcol = NULL;
        json_object* jsev = NULL;
        json_object* jcat = NULL;
        json_object* jcode = NULL;
        json_object* jmsg = NULL;
        if (json_object_object_get_ex(obj, "file", &jfile) &&
            json_object_object_get_ex(obj, "line", &jline) &&
            json_object_object_get_ex(obj, "col", &jcol) &&
            json_object_object_get_ex(obj, "severity", &jsev) &&
            json_object_object_get_ex(obj, "message", &jmsg)) {
            json_object_object_get_ex(obj, "category", &jcat);
            json_object_object_get_ex(obj, "code_id", &jcode);
            addDiagnosticWithMeta(json_object_get_string(jfile),
                                  json_object_get_int(jline),
                                  json_object_get_int(jcol),
                                  json_object_get_string(jmsg),
                                  (DiagnosticSeverity)json_object_get_int(jsev),
                                  jcat ? (DiagnosticCategory)json_object_get_int(jcat) : DIAG_CATEGORY_UNKNOWN,
                                  jcode ? json_object_get_int(jcode) : 0);
        }
    }
    json_object_put(root);
}
