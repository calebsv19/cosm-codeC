#include "diagnostics_engine.h"
#include <errno.h>
#include <json-c/json.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "Compiler/diagnostic_metadata.h"

#define MAX_DIAGNOSTICS 512
#define DIAGNOSTICS_ARTIFACT_MAX_BYTES (1 << 20)

static Diagnostic diagnostics[MAX_DIAGNOSTICS];
static int diagnosticCount = 0;
static DiagnosticIoReport g_last_io_report;

static void diagnostics_io_report_init(DiagnosticIoReport* report,
                                       DiagnosticIoOperation operation,
                                       const char* workspaceRoot) {
    if (!report) return;
    memset(report, 0, sizeof(*report));
    report->operation = operation;
    report->reason = DIAGNOSTIC_IO_REASON_NONE;
    if (workspaceRoot && workspaceRoot[0]) {
        snprintf(report->path,
                 sizeof(report->path),
                 "%s/ide_files/analysis_diagnostics.json",
                 workspaceRoot);
        report->path[sizeof(report->path) - 1] = '\0';
    }
}

static void diagnostics_io_report_finish(DiagnosticIoReport* report,
                                         DiagnosticIoReason reason,
                                         bool ok,
                                         bool noisy_failure) {
    if (!report) return;
    report->reason = reason;
    report->ok = ok;
    report->noisy_failure = noisy_failure;
    g_last_io_report = *report;
    if (noisy_failure) {
        fprintf(stderr,
                "[DiagnosticsIO] %s path=%s\n",
                diagnostics_io_reason_string(reason),
                report->path[0] ? report->path : "(none)");
    }
}

static bool diagnostics_io_build_dir(const char* workspaceRoot,
                                     char* outDir,
                                     size_t outDirCap) {
    if (!workspaceRoot || !workspaceRoot[0] || !outDir || outDirCap == 0) return false;
    snprintf(outDir, outDirCap, "%s/ide_files", workspaceRoot);
    outDir[outDirCap - 1] = '\0';
    return outDir[0] != '\0';
}

const char* diagnostics_io_reason_string(DiagnosticIoReason reason) {
    switch (reason) {
        case DIAGNOSTIC_IO_REASON_INVALID_WORKSPACE: return "invalid_workspace";
        case DIAGNOSTIC_IO_REASON_MISSING_OK: return "missing_ok";
        case DIAGNOSTIC_IO_REASON_SAVED: return "saved";
        case DIAGNOSTIC_IO_REASON_LOADED: return "loaded";
        case DIAGNOSTIC_IO_REASON_SAVE_MKDIR_FAILED: return "save_mkdir_failed";
        case DIAGNOSTIC_IO_REASON_SAVE_SERIALIZE_FAILED: return "save_serialize_failed";
        case DIAGNOSTIC_IO_REASON_SAVE_OPEN_FAILED: return "save_open_failed";
        case DIAGNOSTIC_IO_REASON_SAVE_WRITE_FAILED: return "save_write_failed";
        case DIAGNOSTIC_IO_REASON_LOAD_OPEN_MISSING: return "load_open_missing";
        case DIAGNOSTIC_IO_REASON_LOAD_STAT_FAILED: return "load_stat_failed";
        case DIAGNOSTIC_IO_REASON_LOAD_EMPTY: return "load_empty";
        case DIAGNOSTIC_IO_REASON_LOAD_OVERSIZED: return "load_oversized";
        case DIAGNOSTIC_IO_REASON_LOAD_READ_FAILED: return "load_read_failed";
        case DIAGNOSTIC_IO_REASON_LOAD_ALLOC_FAILED: return "load_alloc_failed";
        case DIAGNOSTIC_IO_REASON_LOAD_INVALID_JSON: return "load_invalid_json";
        case DIAGNOSTIC_IO_REASON_LOAD_INVALID_ROOT: return "load_invalid_root";
        case DIAGNOSTIC_IO_REASON_LOAD_MALFORMED_ROWS: return "load_malformed_rows";
        case DIAGNOSTIC_IO_REASON_NONE:
        default: return "none";
    }
}

const char* diagnostic_category_name(DiagnosticCategory category) {
    switch (category) {
        case DIAG_CATEGORY_BUILD: return "build";
        case DIAG_CATEGORY_ANALYSIS: return "analysis";
        case DIAG_CATEGORY_PARSER: return "parser";
        case DIAG_CATEGORY_SEMANTIC: return "semantic";
        case DIAG_CATEGORY_PREPROCESSOR: return "preprocessor";
        case DIAG_CATEGORY_LEXER: return "lexer";
        case DIAG_CATEGORY_CODEGEN: return "codegen";
        case DIAG_CATEGORY_EXTENSION: return "extension";
        case DIAG_CATEGORY_UNKNOWN:
        default: return "unknown";
    }
}

const char* diagnostic_code_name(int codeId) {
    switch (codeId) {
        case FISICS_DIAG_CODE_UNKNOWN: return "unknown";
        case FISICS_DIAG_CODE_GENERIC: return "generic";
        case FISICS_DIAG_CODE_PARSER_GENERIC: return "parser.generic";
        case FISICS_DIAG_CODE_PARSER_EXPECT_SEMICOLON: return "parser.expect_semicolon";
        case FISICS_DIAG_CODE_SEMANTIC_GENERIC: return "semantic.generic";
        case FISICS_DIAG_CODE_PREPROCESSOR_GENERIC: return "preprocessor.generic";
        case FISICS_DIAG_CODE_EXTENSION_GENERIC: return "extension.generic";
        case FISICS_DIAG_CODE_EXTENSION_UNITS_DISABLED: return "extension.units.disabled";
        case FISICS_DIAG_CODE_EXTENSION_UNITS_INVALID_DIM: return "extension.units.invalid_dim";
        case FISICS_DIAG_CODE_EXTENSION_UNITS_DUPLICATE: return "extension.units.duplicate_dim";
        case FISICS_DIAG_CODE_EXTENSION_UNITS_INVALID_UNIT: return "extension.units.invalid_unit";
        case FISICS_DIAG_CODE_EXTENSION_UNITS_DUPLICATE_UNIT: return "extension.units.duplicate_unit";
        case FISICS_DIAG_CODE_EXTENSION_UNITS_UNIT_REQUIRES_DIM: return "extension.units.unit_requires_dim";
        case FISICS_DIAG_CODE_EXTENSION_UNITS_UNIT_DIM_MISMATCH: return "extension.units.unit_dim_mismatch";
        case FISICS_DIAG_CODE_EXTENSION_UNITS_ADD_DIM_MISMATCH: return "extension.units.add_dim_mismatch";
        case FISICS_DIAG_CODE_EXTENSION_UNITS_SUB_DIM_MISMATCH: return "extension.units.sub_dim_mismatch";
        case FISICS_DIAG_CODE_EXTENSION_UNITS_ASSIGN_DIM_MISMATCH: return "extension.units.assign_dim_mismatch";
        case FISICS_DIAG_CODE_EXTENSION_UNITS_COMPARE_DIM_MISMATCH: return "extension.units.compare_dim_mismatch";
        case FISICS_DIAG_CODE_EXTENSION_UNITS_EXPONENT_OVERFLOW: return "extension.units.exponent_overflow";
        case FISICS_DIAG_CODE_EXTENSION_UNITS_UNSUPPORTED_OPERATION: return "extension.units.unsupported_operation";
        case FISICS_DIAG_CODE_EXTENSION_UNITS_IMPLICIT_CONCRETE_CONVERSION: return "extension.units.implicit_concrete_conversion";
        case FISICS_DIAG_CODE_EXTENSION_UNITS_CONVERSION_INVALID_TARGET: return "extension.units.conversion_invalid_target";
        case FISICS_DIAG_CODE_EXTENSION_UNITS_CONVERSION_INCOMPATIBLE: return "extension.units.conversion_incompatible";
        case FISICS_DIAG_CODE_EXTENSION_UNITS_CONVERSION_REQUIRES_SOURCE_UNIT: return "extension.units.conversion_requires_source_unit";
        case FISICS_DIAG_CODE_EXTENSION_UNITS_CONVERSION_REQUIRES_FLOATING: return "extension.units.conversion_requires_floating";
        case FISICS_DIAG_CODE_LEXER_GENERIC: return "lexer.generic";
        case FISICS_DIAG_CODE_CODEGEN_GENERIC: return "codegen.generic";
        case FISICS_DIAG_CODE_BUILD_GENERIC: return "build.generic";
        case FISICS_DIAG_CODE_BUILD_DRIVER_GENERIC: return "build.driver_generic";
        case FISICS_DIAG_CODE_LINK_GENERIC: return "link.generic";
        case FISICS_DIAG_CODE_LINK_STAGE_FAILED: return "link.stage_failed";
        case FISICS_DIAG_CODE_BUILD_MANIFEST_GENERIC: return "build.manifest_generic";
        case FISICS_DIAG_CODE_BUILD_MANIFEST_LOAD_FAILED: return "build.manifest_load_failed";
        default: return "unknown";
    }
}

const char* diagnostic_stage_name(int codeId) {
    if (codeId >= 7200 && codeId <= 7299) return "build";
    if (codeId >= 7100 && codeId <= 7199) return "link";
    if (codeId >= 7000 && codeId <= 7099) return "build";
    if (codeId >= 6000 && codeId <= 6099) return "codegen";
    if (codeId >= 5000 && codeId <= 5099) return "lex";
    if (codeId >= FISICS_DIAG_CODE_EXTENSION_GENERIC &&
        codeId <= FISICS_DIAG_CODE_EXTENSION_UNITS_CONVERSION_REQUIRES_FLOATING) {
        return "extension";
    }
    if (codeId >= FISICS_DIAG_CODE_PREPROCESSOR_GENERIC &&
        codeId < FISICS_DIAG_CODE_EXTENSION_GENERIC) {
        return "preprocess";
    }
    if (codeId >= FISICS_DIAG_CODE_SEMANTIC_GENERIC &&
        codeId < FISICS_DIAG_CODE_PREPROCESSOR_GENERIC) {
        return "semantic";
    }
    if (codeId >= FISICS_DIAG_CODE_PARSER_GENERIC &&
        codeId < FISICS_DIAG_CODE_SEMANTIC_GENERIC) {
        return "parse";
    }
    return "unknown";
}

static char* dup_or_null(const char* text) {
    return (text && text[0]) ? strdup(text) : NULL;
}

static void free_diagnostic(Diagnostic* d) {
    if (!d) return;
    free((char*)d->filePath);
    free((char*)d->message);
    free((char*)d->hint);
    free((char*)d->codeName);
    free((char*)d->stage);
    memset(d, 0, sizeof(*d));
}

void initDiagnosticsEngine() {
    clearDiagnostics();
    memset(&g_last_io_report, 0, sizeof(g_last_io_report));
}

void clearDiagnostics() {
    for (int i = 0; i < diagnosticCount; ++i) {
        free_diagnostic(&diagnostics[i]);
    }
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
    addDiagnosticWithDetails(file,
                             line,
                             col,
                             1,
                             msg,
                             NULL,
                             severity,
                             category,
                             codeId,
                             NULL,
                             NULL);
}

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
                              const char* stage) {
    if (diagnosticCount >= MAX_DIAGNOSTICS) return;

    Diagnostic* d = &diagnostics[diagnosticCount];
    memset(d, 0, sizeof(*d));
    d->filePath = strdup(file ? file : "");
    d->line = line;
    d->column = col;
    d->length = length > 0 ? length : 1;
    d->message = strdup(msg ? msg : "");
    d->hint = dup_or_null(hint);
    d->severity = severity;
    d->category = category;
    d->codeId = codeId;
    const char* resolvedCodeName = (codeName && codeName[0]) ? codeName : diagnostic_code_name(codeId);
    const char* resolvedStage = (stage && stage[0]) ? stage : diagnostic_stage_name(codeId);
    d->codeName = dup_or_null(resolvedCodeName);
    d->stage = dup_or_null(resolvedStage);

    diagnosticCount++;
}

int getDiagnosticCount() {
    return diagnosticCount;
}

const Diagnostic* getDiagnosticAt(int index) {
    if (index < 0 || index >= diagnosticCount) return NULL;
    return &diagnostics[index];
}

bool diagnostics_save_report(const char* workspaceRoot, DiagnosticIoReport* outReport) {
    DiagnosticIoReport report;
    diagnostics_io_report_init(&report, DIAGNOSTIC_IO_SAVE, workspaceRoot);
    if (!workspaceRoot || !*workspaceRoot) {
        diagnostics_io_report_finish(&report, DIAGNOSTIC_IO_REASON_INVALID_WORKSPACE, false, true);
        if (outReport) *outReport = report;
        return false;
    }

    char dir[1024];
    if (!diagnostics_io_build_dir(workspaceRoot, dir, sizeof(dir))) {
        diagnostics_io_report_finish(&report, DIAGNOSTIC_IO_REASON_INVALID_WORKSPACE, false, true);
        if (outReport) *outReport = report;
        return false;
    }
    struct stat st;
    if (stat(dir, &st) == 0 && !S_ISDIR(st.st_mode)) {
        diagnostics_io_report_finish(&report, DIAGNOSTIC_IO_REASON_SAVE_MKDIR_FAILED, false, true);
        if (outReport) *outReport = report;
        return false;
    }
    if (stat(dir, &st) != 0) {
        if (mkdir(dir, 0755) != 0 && errno != EEXIST) {
            diagnostics_io_report_finish(&report, DIAGNOSTIC_IO_REASON_SAVE_MKDIR_FAILED, false, true);
            if (outReport) *outReport = report;
            return false;
        }
    }

    json_object* arr = json_object_new_array();
    if (!arr) {
        diagnostics_io_report_finish(&report, DIAGNOSTIC_IO_REASON_SAVE_SERIALIZE_FAILED, false, true);
        if (outReport) *outReport = report;
        return false;
    }
    for (int i = 0; i < diagnosticCount; ++i) {
        const Diagnostic* d = &diagnostics[i];
        json_object* obj = json_object_new_object();
        if (!obj) {
            json_object_put(arr);
            diagnostics_io_report_finish(&report, DIAGNOSTIC_IO_REASON_SAVE_SERIALIZE_FAILED, false, true);
            if (outReport) *outReport = report;
            return false;
        }
        json_object_object_add(obj, "file", json_object_new_string(d->filePath ? d->filePath : ""));
        json_object_object_add(obj, "line", json_object_new_int(d->line));
        json_object_object_add(obj, "col", json_object_new_int(d->column));
        json_object_object_add(obj, "length", json_object_new_int(d->length > 0 ? d->length : 1));
        json_object_object_add(obj, "severity", json_object_new_int(d->severity));
        json_object_object_add(obj, "category", json_object_new_int((int)d->category));
        json_object_object_add(obj, "code_id", json_object_new_int(d->codeId));
        json_object_object_add(obj, "code_name", json_object_new_string(d->codeName ? d->codeName : ""));
        json_object_object_add(obj, "stage", json_object_new_string(d->stage ? d->stage : ""));
        json_object_object_add(obj, "message", json_object_new_string(d->message ? d->message : ""));
        json_object_object_add(obj, "hint", json_object_new_string(d->hint ? d->hint : ""));
        json_object_array_add(arr, obj);
    }

    const char* serialized = json_object_to_json_string_ext(arr, JSON_C_TO_STRING_PLAIN);
    if (!serialized) {
        json_object_put(arr);
        diagnostics_io_report_finish(&report, DIAGNOSTIC_IO_REASON_SAVE_SERIALIZE_FAILED, false, true);
        if (outReport) *outReport = report;
        return false;
    }

    FILE* f = fopen(report.path, "w");
    if (!f) {
        json_object_put(arr);
        diagnostics_io_report_finish(&report, DIAGNOSTIC_IO_REASON_SAVE_OPEN_FAILED, false, true);
        if (outReport) *outReport = report;
        return false;
    }

    size_t serialized_len = strlen(serialized);
    bool write_ok = (fwrite(serialized, 1, serialized_len, f) == serialized_len);
    if (fclose(f) != 0) write_ok = false;
    json_object_put(arr);
    if (!write_ok) {
        diagnostics_io_report_finish(&report, DIAGNOSTIC_IO_REASON_SAVE_WRITE_FAILED, false, true);
        if (outReport) *outReport = report;
        return false;
    }

    report.saved_rows = diagnosticCount;
    diagnostics_io_report_finish(&report, DIAGNOSTIC_IO_REASON_SAVED, true, false);
    if (outReport) *outReport = report;
    return true;
}

void diagnostics_save(const char* workspaceRoot) {
    (void)diagnostics_save_report(workspaceRoot, NULL);
}

bool diagnostics_load_report(const char* workspaceRoot, DiagnosticIoReport* outReport) {
    DiagnosticIoReport report;
    diagnostics_io_report_init(&report, DIAGNOSTIC_IO_LOAD, workspaceRoot);
    clearDiagnostics();
    if (!workspaceRoot || !*workspaceRoot) {
        diagnostics_io_report_finish(&report, DIAGNOSTIC_IO_REASON_INVALID_WORKSPACE, false, true);
        if (outReport) *outReport = report;
        return false;
    }

    struct stat st;
    if (stat(report.path, &st) != 0) {
        bool missing = (errno == ENOENT);
        diagnostics_io_report_finish(&report,
                                     missing
                                         ? DIAGNOSTIC_IO_REASON_MISSING_OK
                                         : DIAGNOSTIC_IO_REASON_LOAD_OPEN_MISSING,
                                     missing,
                                     !missing);
        if (outReport) *outReport = report;
        return missing;
    }
    if (!S_ISREG(st.st_mode)) {
        diagnostics_io_report_finish(&report, DIAGNOSTIC_IO_REASON_LOAD_STAT_FAILED, false, true);
        if (outReport) *outReport = report;
        return false;
    }
    report.size_bytes = (long)st.st_size;
    if (st.st_size <= 0) {
        diagnostics_io_report_finish(&report, DIAGNOSTIC_IO_REASON_LOAD_EMPTY, false, true);
        if (outReport) *outReport = report;
        return false;
    }
    if (st.st_size > DIAGNOSTICS_ARTIFACT_MAX_BYTES) {
        diagnostics_io_report_finish(&report, DIAGNOSTIC_IO_REASON_LOAD_OVERSIZED, false, true);
        if (outReport) *outReport = report;
        return false;
    }

    FILE* f = fopen(report.path, "r");
    if (!f) {
        diagnostics_io_report_finish(&report, DIAGNOSTIC_IO_REASON_LOAD_OPEN_MISSING, false, true);
        if (outReport) *outReport = report;
        return false;
    }

    char* buf = malloc((size_t)st.st_size + 1);
    if (!buf) {
        fclose(f);
        diagnostics_io_report_finish(&report, DIAGNOSTIC_IO_REASON_LOAD_ALLOC_FAILED, false, true);
        if (outReport) *outReport = report;
        return false;
    }
    size_t nread = fread(buf, 1, (size_t)st.st_size, f);
    bool read_ok = (nread == (size_t)st.st_size);
    if (ferror(f)) read_ok = false;
    if (fclose(f) != 0) read_ok = false;
    if (!read_ok) {
        free(buf);
        diagnostics_io_report_finish(&report, DIAGNOSTIC_IO_REASON_LOAD_READ_FAILED, false, true);
        if (outReport) *outReport = report;
        return false;
    }
    buf[nread] = '\0';

    json_object* root = json_tokener_parse(buf);
    free(buf);
    if (!root) {
        diagnostics_io_report_finish(&report, DIAGNOSTIC_IO_REASON_LOAD_INVALID_JSON, false, true);
        if (outReport) *outReport = report;
        return false;
    }
    if (!json_object_is_type(root, json_type_array)) {
        json_object_put(root);
        diagnostics_io_report_finish(&report, DIAGNOSTIC_IO_REASON_LOAD_INVALID_ROOT, false, true);
        if (outReport) *outReport = report;
        return false;
    }

    size_t arrLen = json_object_array_length(root);
    for (size_t i = 0; i < arrLen && diagnosticCount < MAX_DIAGNOSTICS; ++i) {
        json_object* obj = json_object_array_get_idx(root, i);
        if (!obj) {
            report.malformed_rows++;
            continue;
        }
        json_object* jfile = NULL;
        json_object* jline = NULL;
        json_object* jcol = NULL;
        json_object* jsev = NULL;
        json_object* jcat = NULL;
        json_object* jcode = NULL;
        json_object* jcode_name = NULL;
        json_object* jstage = NULL;
        json_object* jlen = NULL;
        json_object* jhint = NULL;
        json_object* jmsg = NULL;
        if (json_object_object_get_ex(obj, "file", &jfile) &&
            json_object_object_get_ex(obj, "line", &jline) &&
            json_object_object_get_ex(obj, "col", &jcol) &&
            json_object_object_get_ex(obj, "severity", &jsev) &&
            json_object_object_get_ex(obj, "message", &jmsg)) {
            json_object_object_get_ex(obj, "category", &jcat);
            json_object_object_get_ex(obj, "code_id", &jcode);
            json_object_object_get_ex(obj, "code_name", &jcode_name);
            json_object_object_get_ex(obj, "stage", &jstage);
            json_object_object_get_ex(obj, "length", &jlen);
            json_object_object_get_ex(obj, "hint", &jhint);
            addDiagnosticWithDetails(json_object_get_string(jfile),
                                     json_object_get_int(jline),
                                     json_object_get_int(jcol),
                                     jlen ? json_object_get_int(jlen) : 1,
                                     json_object_get_string(jmsg),
                                     jhint ? json_object_get_string(jhint) : NULL,
                                     (DiagnosticSeverity)json_object_get_int(jsev),
                                     jcat ? (DiagnosticCategory)json_object_get_int(jcat) : DIAG_CATEGORY_UNKNOWN,
                                     jcode ? json_object_get_int(jcode) : 0,
                                     jcode_name ? json_object_get_string(jcode_name) : NULL,
                                     jstage ? json_object_get_string(jstage) : NULL);
            report.loaded_rows++;
        } else {
            report.malformed_rows++;
        }
    }
    json_object_put(root);
    diagnostics_io_report_finish(&report,
                                 report.malformed_rows > 0
                                     ? DIAGNOSTIC_IO_REASON_LOAD_MALFORMED_ROWS
                                     : DIAGNOSTIC_IO_REASON_LOADED,
                                 true,
                                 report.malformed_rows > 0);
    if (outReport) *outReport = report;
    return true;
}

void diagnostics_load(const char* workspaceRoot) {
    (void)diagnostics_load_report(workspaceRoot, NULL);
}

void diagnostics_last_io_report(DiagnosticIoReport* outReport) {
    if (!outReport) return;
    *outReport = g_last_io_report;
}
