#include "ide/Panes/ToolPanels/Errors/errors_context_detail.h"

#include "core/Diagnostics/diagnostic_context.h"

#include <json-c/json.h>
#include <stdio.h>
#include <string.h>

static const char* diagnostic_effective_code_name_local(const Diagnostic* diag) {
    if (!diag) return NULL;
    return (diag->codeName && diag->codeName[0])
        ? diag->codeName
        : diagnostic_code_name(diag->codeId);
}

static const DiagnosticContextRecord* find_diagnostic_context_local(const Diagnostic* diag) {
    if (!diag) return NULL;
    return diagnostic_context_find(diag->filePath,
                                   diag->line,
                                   diag->column,
                                   diag->codeId,
                                   diagnostic_effective_code_name_local(diag),
                                   diag->message);
}

static int json_array_count_or_zero(const char* jsonText) {
    if (!jsonText || !jsonText[0]) return 0;
    json_object* parsed = json_tokener_parse(jsonText);
    if (!parsed) return 0;
    int count = json_object_is_type(parsed, json_type_array)
        ? (int)json_object_array_length(parsed)
        : 0;
    json_object_put(parsed);
    return count;
}

static const char* basename_for_display(const char* path) {
    if (!path || !path[0]) return "(unknown)";
    const char* slash = strrchr(path, '/');
    const char* backslash = strrchr(path, '\\');
    const char* cut = slash > backslash ? slash : backslash;
    return cut ? cut + 1 : path;
}

static const char* json_string_field(json_object* obj, const char* key) {
    if (!obj || !key) return NULL;
    json_object* value = NULL;
    if (!json_object_object_get_ex(obj, key, &value)) return NULL;
    const char* text = json_object_get_string(value);
    return (text && text[0]) ? text : NULL;
}

static int json_int_field_or(json_object* obj, const char* key, int fallback) {
    if (!obj || !key) return fallback;
    json_object* value = NULL;
    if (!json_object_object_get_ex(obj, key, &value)) return fallback;
    int n = json_object_get_int(value);
    return n > 0 ? n : fallback;
}

static ErrorsContextDetailRow* append_row(ErrorsContextDetail* detail,
                                          ErrorsContextRowKind kind) {
    if (!detail || detail->rowCount >= ERRORS_CONTEXT_DETAIL_MAX_ROWS) return NULL;
    ErrorsContextDetailRow* row = &detail->rows[detail->rowCount++];
    memset(row, 0, sizeof(*row));
    row->kind = kind;
    row->targetLine = 1;
    row->targetColumn = 1;
    return row;
}

static void capture_first_target(ErrorsContextDetail* detail,
                                 const ErrorsContextDetailRow* row,
                                 const char* kind) {
    if (!detail || !row || !row->hasNavigationTarget || detail->hasNavigationTarget) return;
    snprintf(detail->targetPath, sizeof(detail->targetPath), "%s", row->targetPath);
    snprintf(detail->targetKind, sizeof(detail->targetKind), "%s", kind ? kind : "context");
    detail->targetLine = row->targetLine;
    detail->targetColumn = row->targetColumn;
    detail->hasNavigationTarget = true;
}

static int append_context_array_rows(ErrorsContextDetail* detail,
                                     const char* jsonText,
                                     ErrorsContextRowKind kind,
                                     const char* label,
                                     int maxRows) {
    if (!detail || !jsonText || !jsonText[0] || maxRows <= 0) return 0;
    json_object* parsed = json_tokener_parse(jsonText);
    if (!parsed) return 0;
    if (!json_object_is_type(parsed, json_type_array)) {
        json_object_put(parsed);
        return 0;
    }

    int appended = 0;
    int count = (int)json_object_array_length(parsed);
    for (int i = 0; i < count && appended < maxRows; ++i) {
        json_object* item = json_object_array_get_idx(parsed, (size_t)i);
        if (!item || !json_object_is_type(item, json_type_object)) continue;

        const char* file = json_string_field(item, "file");
        const char* role = json_string_field(item, "role");
        const char* macro = json_string_field(item, "macro");
        const char* origin = json_string_field(item, "origin");
        int line = json_int_field_or(item, "line", 1);
        int column = json_int_field_or(item, "column", 1);

        ErrorsContextDetailRow* row = append_row(detail, kind);
        if (!row) break;
        if (file && file[0]) {
            row->hasNavigationTarget = true;
            snprintf(row->targetPath, sizeof(row->targetPath), "%s", file);
            row->targetLine = line;
            row->targetColumn = column;
        }

        if (kind == ERRORS_CONTEXT_ROW_MACRO) {
            snprintf(row->text,
                     sizeof(row->text),
                     "%s %d: %s%s%s%s%s%s%s:%d:%d",
                     label ? label : "macro",
                     i + 1,
                     macro ? macro : "(macro)",
                     role ? " " : "",
                     role ? role : "",
                     origin ? " " : "",
                     origin ? origin : "",
                     file ? " at " : "",
                     file ? basename_for_display(file) : "(no source)",
                     line,
                     column);
        } else {
            snprintf(row->text,
                     sizeof(row->text),
                     "%s %d: %s%s%s%s:%d:%d",
                     label ? label : "include",
                     i + 1,
                     origin ? origin : basename_for_display(file),
                     file ? " at " : "",
                     file ? basename_for_display(file) : "(no source)",
                     file ? "" : "",
                     line,
                     column);
        }

        capture_first_target(detail, row, kind == ERRORS_CONTEXT_ROW_MACRO ? "macro" : "include");
        appended++;
    }

    if (count > appended && detail->rowCount < ERRORS_CONTEXT_DETAIL_MAX_ROWS) {
        ErrorsContextDetailRow* row = append_row(detail, kind);
        if (row) {
            snprintf(row->text,
                     sizeof(row->text),
                     "%s: %d more",
                     label ? label : "context",
                     count - appended);
        }
    }

    json_object_put(parsed);
    return appended;
}

static void append_details_row(ErrorsContextDetail* detail, const char* detailsJson) {
    if (!detail || !detailsJson || !detailsJson[0]) return;
    ErrorsContextDetailRow* row = append_row(detail, ERRORS_CONTEXT_ROW_DETAILS);
    if (!row) return;

    json_object* parsed = json_tokener_parse(detailsJson);
    if (parsed && json_object_is_type(parsed, json_type_object)) {
        const char* context = json_string_field(parsed, "context");
        const char* expected = json_string_field(parsed, "expected_dim_text");
        const char* actual = json_string_field(parsed, "actual_dim_text");
        if (!expected) expected = json_string_field(parsed, "lhs_dim_text");
        if (!actual) actual = json_string_field(parsed, "rhs_dim_text");
        if (expected || actual || context) {
            snprintf(row->text,
                     sizeof(row->text),
                     "details: %s%s%s%s%s",
                     context ? context : "diagnostic",
                     expected ? " expected " : "",
                     expected ? expected : "",
                     actual ? " actual " : "",
                     actual ? actual : "");
            json_object_put(parsed);
            return;
        }
    }

    snprintf(row->text, sizeof(row->text), "details: structured payload available");
    if (parsed) json_object_put(parsed);
}

bool errors_context_detail_for_diagnostic(const Diagnostic* diag,
                                          ErrorsContextDetail* outDetail) {
    if (!outDetail) return false;
    memset(outDetail, 0, sizeof(*outDetail));
    const DiagnosticContextRecord* context = find_diagnostic_context_local(diag);
    if (!context) return false;

    outDetail->includeCount = json_array_count_or_zero(context->includeStackJson);
    outDetail->macroCount = json_array_count_or_zero(context->macroTraceJson);
    outDetail->hasDetails = context->detailsJson && context->detailsJson[0];
    if (outDetail->includeCount <= 0 && outDetail->macroCount <= 0 && !outDetail->hasDetails) {
        return false;
    }

    ErrorsContextDetailRow* summary = append_row(outDetail, ERRORS_CONTEXT_ROW_SUMMARY);
    if (summary) {
        snprintf(summary->text,
                 sizeof(summary->text),
                 "context: includes %d   macros %d%s",
                 outDetail->includeCount,
                 outDetail->macroCount,
                 outDetail->hasDetails ? "   details" : "");
    }

    int remaining = ERRORS_CONTEXT_DETAIL_MAX_ROWS - outDetail->rowCount;
    int includeRows = remaining > 4 ? 4 : remaining;
    append_context_array_rows(outDetail,
                              context->includeStackJson,
                              ERRORS_CONTEXT_ROW_INCLUDE,
                              "include",
                              includeRows);

    remaining = ERRORS_CONTEXT_DETAIL_MAX_ROWS - outDetail->rowCount;
    int macroRows = remaining > 4 ? 4 : remaining;
    append_context_array_rows(outDetail,
                              context->macroTraceJson,
                              ERRORS_CONTEXT_ROW_MACRO,
                              "macro",
                              macroRows);

    if (outDetail->hasDetails) {
        append_details_row(outDetail, context->detailsJson);
    }

    return outDetail->rowCount > 0;
}
