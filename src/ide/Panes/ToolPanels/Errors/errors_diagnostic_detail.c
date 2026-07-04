#include "ide/Panes/ToolPanels/Errors/errors_diagnostic_detail.h"

#include "core/Diagnostics/diagnostic_explanations.h"
#include "ide/Panes/ToolPanels/Errors/errors_context_detail.h"
#include "ide/Panes/ToolPanels/Errors/errors_units_detail.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static const char* detail_severity_label(const Diagnostic* diag) {
    if (!diag) return "Info";
    return (diag->severity == DIAG_SEVERITY_ERROR)
        ? "Error"
        : (diag->severity == DIAG_SEVERITY_WARNING) ? "Warning" : "Info";
}

static const char* detail_display_path(const Diagnostic* diag, const char* displayPath) {
    if (displayPath && displayPath[0]) return displayPath;
    if (diag && diag->filePath && diag->filePath[0]) return diag->filePath;
    return "(unknown file)";
}

static const DiagnosticExplanation* detail_find_explanation(const Diagnostic* diag,
                                                            const char* codeName) {
    if (!diag) return NULL;
    const DiagnosticExplanation* explanation = diagnostic_explanations_find_by_code(diag->codeId);
    if (!explanation && codeName && codeName[0]) {
        explanation = diagnostic_explanations_find_by_name(codeName);
    }
    return explanation;
}

static bool detail_add_line(ErrorsDiagnosticDetailModel* model,
                            ErrorsDiagnosticDetailLineKind kind,
                            bool hasNavigationTarget,
                            const char* fmt,
                            ...) {
    if (!model || !fmt || model->lineCount >= ERRORS_DIAGNOSTIC_DETAIL_MAX_LINES) {
        return false;
    }

    ErrorsDiagnosticDetailLine* line = &model->lines[model->lineCount++];
    line->kind = kind;
    line->hasNavigationTarget = hasNavigationTarget;

    va_list args;
    va_start(args, fmt);
    vsnprintf(line->text, sizeof(line->text), fmt, args);
    va_end(args);

    return true;
}

bool errors_diagnostic_detail_build(const Diagnostic* diag,
                                    const char* displayPath,
                                    ErrorsDiagnosticDetailModel* outModel) {
    if (!diag || !outModel) return false;
    memset(outModel, 0, sizeof(*outModel));

    const char* path = detail_display_path(diag, displayPath);
    const char* category = diagnostic_category_name(diag->category);
    const char* codeName = (diag->codeName && diag->codeName[0])
        ? diag->codeName
        : diagnostic_code_name(diag->codeId);
    const char* stage = (diag->stage && diag->stage[0])
        ? diag->stage
        : diagnostic_stage_name(diag->codeId);

    detail_add_line(outModel,
                    ERRORS_DIAGNOSTIC_DETAIL_LINE_TITLE,
                    false,
                    "Diagnostic Detail  %s",
                    detail_severity_label(diag));

    if (diag->length > 0) {
        detail_add_line(outModel,
                        ERRORS_DIAGNOSTIC_DETAIL_LINE_META,
                        false,
                        "%s:%d:%d span %d",
                        path,
                        diag->line,
                        diag->column,
                        diag->length);
    } else {
        detail_add_line(outModel,
                        ERRORS_DIAGNOSTIC_DETAIL_LINE_META,
                        false,
                        "%s:%d:%d",
                        path,
                        diag->line,
                        diag->column);
    }

    detail_add_line(outModel,
                    ERRORS_DIAGNOSTIC_DETAIL_LINE_META,
                    false,
                    "category: %s   code: %s (%d)   stage: %s",
                    category ? category : "unknown",
                    (codeName && codeName[0]) ? codeName : "unknown",
                    diag->codeId,
                    (stage && stage[0]) ? stage : "unknown");

    detail_add_line(outModel,
                    ERRORS_DIAGNOSTIC_DETAIL_LINE_MESSAGE,
                    false,
                    "%s",
                    diag->message ? diag->message : "(no message)");

    if (diag->hint && diag->hint[0]) {
        detail_add_line(outModel,
                        ERRORS_DIAGNOSTIC_DETAIL_LINE_HINT,
                        false,
                        "hint: %s",
                        diag->hint);
    }

    const DiagnosticExplanation* explanation = detail_find_explanation(diag, codeName);
    if (explanation && explanation->description && explanation->description[0]) {
        detail_add_line(outModel,
                        ERRORS_DIAGNOSTIC_DETAIL_LINE_EXPLANATION,
                        false,
                        "why: %s",
                        explanation->description);
    }

    ErrorsUnitsDiagnosticDetail unitsDetail;
    if (errors_units_detail_for_diagnostic(diag, &unitsDetail)) {
        if (unitsDetail.hasSymbolUnits) {
            detail_add_line(outModel,
                            ERRORS_DIAGNOSTIC_DETAIL_LINE_UNITS,
                            false,
                            "units: %s   %s %s -> %s   symbol %s %s",
                            unitsDetail.context[0] ? unitsDetail.context : "detail",
                            unitsDetail.leftDimText[0] ? unitsDetail.leftLabel : "dim",
                            unitsDetail.leftDimText,
                            unitsDetail.rightDimText,
                            unitsDetail.symbolName[0]
                                ? unitsDetail.symbolName
                                : unitsDetail.symbolStableId,
                            unitsDetail.unitText);
        } else {
            detail_add_line(outModel,
                            ERRORS_DIAGNOSTIC_DETAIL_LINE_UNITS,
                            false,
                            "units: %s   %s %s -> %s",
                            unitsDetail.context[0] ? unitsDetail.context : "detail",
                            unitsDetail.leftDimText[0] ? unitsDetail.leftLabel : "dim",
                            unitsDetail.leftDimText,
                            unitsDetail.rightDimText);
        }
    }

    ErrorsContextDetail contextDetail;
    if (errors_context_detail_for_diagnostic(diag, &contextDetail)) {
        for (int i = 0; i < contextDetail.rowCount; ++i) {
            const ErrorsContextDetailRow* row = &contextDetail.rows[i];
            const char* prefix = "";
            if (row->kind == ERRORS_CONTEXT_ROW_INCLUDE ||
                row->kind == ERRORS_CONTEXT_ROW_MACRO ||
                row->kind == ERRORS_CONTEXT_ROW_DETAILS) {
                prefix = "  ";
            }
            detail_add_line(outModel,
                            row->hasNavigationTarget
                                ? ERRORS_DIAGNOSTIC_DETAIL_LINE_NAV_CONTEXT
                                : ERRORS_DIAGNOSTIC_DETAIL_LINE_CONTEXT,
                            row->hasNavigationTarget,
                            "%s%s%s",
                            prefix,
                            row->text,
                            row->hasNavigationTarget ? "   open" : "");
        }
    }

    return outModel->lineCount > 0;
}
