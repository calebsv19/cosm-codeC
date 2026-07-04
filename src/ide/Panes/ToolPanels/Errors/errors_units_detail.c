#include "ide/Panes/ToolPanels/Errors/errors_units_detail.h"

#include "core/Analysis/analysis_units_store.h"
#include "core/Diagnostics/diagnostic_context.h"

#include <json-c/json.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* effective_code_name(const Diagnostic* diag) {
    if (!diag) return NULL;
    return (diag->codeName && diag->codeName[0])
        ? diag->codeName
        : diagnostic_code_name(diag->codeId);
}

static const DiagnosticContextRecord* find_context(const Diagnostic* diag) {
    if (!diag) return NULL;
    return diagnostic_context_find(diag->filePath,
                                   diag->line,
                                   diag->column,
                                   diag->codeId,
                                   effective_code_name(diag),
                                   diag->message);
}

static bool code_looks_like_units(const Diagnostic* diag) {
    const char* codeName = effective_code_name(diag);
    return codeName && strstr(codeName, "units");
}

static const char* object_string(json_object* obj, const char* key) {
    json_object* value = NULL;
    if (!obj || !json_object_object_get_ex(obj, key, &value)) return NULL;
    const char* text = json_object_get_string(value);
    return (text && text[0]) ? text : NULL;
}

static uint64_t object_u64(json_object* obj, const char* key) {
    json_object* value = NULL;
    if (!obj || !json_object_object_get_ex(obj, key, &value)) return 0;
    if (json_object_is_type(value, json_type_int)) {
        long long raw = json_object_get_int64(value);
        return raw < 0 ? 0 : (uint64_t)raw;
    }
    if (json_object_is_type(value, json_type_string)) {
        const char* text = json_object_get_string(value);
        if (!text || !text[0]) return 0;
        char* end = NULL;
        unsigned long long parsed = strtoull(text, &end, 0);
        if (!end || *end != '\0') return 0;
        return (uint64_t)parsed;
    }
    return 0;
}

static void copy_text(char* dst, size_t dstSize, const char* src) {
    if (!dst || dstSize == 0) return;
    snprintf(dst, dstSize, "%s", src ? src : "");
}

static bool copy_dimension_pair(json_object* details, ErrorsUnitsDiagnosticDetail* outDetail) {
    const char* left = object_string(details, "lhs_dim_text");
    const char* right = object_string(details, "rhs_dim_text");
    const char* leftLabel = "lhs";
    const char* rightLabel = "rhs";

    if (!left && !right) {
        left = object_string(details, "expected_dim_text");
        right = object_string(details, "actual_dim_text");
        leftLabel = "expected";
        rightLabel = "actual";
    }
    if (!left && !right) return false;

    copy_text(outDetail->leftLabel, sizeof(outDetail->leftLabel), leftLabel);
    copy_text(outDetail->leftDimText, sizeof(outDetail->leftDimText), left);
    copy_text(outDetail->rightLabel, sizeof(outDetail->rightLabel), rightLabel);
    copy_text(outDetail->rightDimText, sizeof(outDetail->rightDimText), right);
    outDetail->hasDimensionDetail = true;
    return true;
}

static uint64_t details_symbol_id(json_object* details) {
    uint64_t id = object_u64(details, "symbol_stable_id");
    if (id == 0) id = object_u64(details, "related_symbol_stable_id");
    if (id == 0) id = object_u64(details, "stable_id");
    return id;
}

static void copy_units_attachment(uint64_t symbolId, ErrorsUnitsDiagnosticDetail* outDetail) {
    if (symbolId == 0 || !outDetail) return;

    analysis_units_store_lock();
    const AnalysisUnitsAttachment* units = analysis_units_store_find_by_symbol_id(symbolId);
    if (units) {
        snprintf(outDetail->symbolStableId,
                 sizeof(outDetail->symbolStableId),
                 "0x%016llx",
                 (unsigned long long)symbolId);
        copy_text(outDetail->symbolName, sizeof(outDetail->symbolName), units->symbol_name);
        if (units->has_concrete_unit && units->unit_symbol && units->unit_symbol[0]) {
            copy_text(outDetail->unitText, sizeof(outDetail->unitText), units->unit_symbol);
        } else if (units->dim_text && units->dim_text[0]) {
            copy_text(outDetail->unitText, sizeof(outDetail->unitText), units->dim_text);
        }
        copy_text(outDetail->unitFamily, sizeof(outDetail->unitFamily), units->unit_family);
        outDetail->hasSymbolUnits = true;
    }
    analysis_units_store_unlock();
}

bool errors_units_detail_for_diagnostic(const Diagnostic* diag,
                                        ErrorsUnitsDiagnosticDetail* outDetail) {
    if (!outDetail) return false;
    memset(outDetail, 0, sizeof(*outDetail));
    if (!diag) return false;

    const DiagnosticContextRecord* context = find_context(diag);
    if (!context || !context->detailsJson || !context->detailsJson[0]) return false;

    json_object* details = json_tokener_parse(context->detailsJson);
    if (!details || !json_object_is_type(details, json_type_object)) {
        if (details) json_object_put(details);
        return false;
    }

    const char* contextText = object_string(details, "context");
    copy_text(outDetail->context, sizeof(outDetail->context), contextText);
    bool hasKnownUnitsShape = copy_dimension_pair(details, outDetail);
    if (hasKnownUnitsShape || code_looks_like_units(diag)) {
        copy_units_attachment(details_symbol_id(details), outDetail);
    }

    json_object_put(details);
    return outDetail->hasDimensionDetail || outDetail->hasSymbolUnits;
}
