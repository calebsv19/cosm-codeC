#include "core/Analysis/analysis_units_store.h"
#include "core/Diagnostics/diagnostic_context.h"
#include "ide/Panes/ToolPanels/Errors/errors_units_detail.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static const char* k_context_json =
    "{"
    "\"diagnostics\":["
    "{"
    "\"file\":\"/tmp/errors_units_detail/units.c\","
    "\"line\":8,\"column\":7,\"length\":6,"
    "\"code_id\":4103,\"code_name\":\"extension.units.assign_dim_mismatch\","
    "\"message\":\"assignment dimension mismatch\","
    "\"details\":{"
    "\"context\":\"assignment\","
    "\"lhs_dim_text\":\"m\","
    "\"lhs_dim\":[1,0,0,0,0,0,0,0],"
    "\"rhs_dim_text\":\"s\","
    "\"rhs_dim\":[0,0,1,0,0,0,0,0],"
    "\"symbol_stable_id\":\"0x1111111111111111\""
    "}"
    "}"
    "]"
    "}";

int main(void) {
    diagnostic_context_clear();
    analysis_units_store_clear();
    assert(diagnostic_context_load_json_text(k_context_json));

    Diagnostic diag = {
        .filePath = "/tmp/errors_units_detail/units.c",
        .line = 8,
        .column = 7,
        .length = 6,
        .message = "assignment dimension mismatch",
        .hint = "convert to compatible units",
        .severity = DIAG_SEVERITY_ERROR,
        .category = DIAG_CATEGORY_EXTENSION,
        .codeId = 4103,
        .codeName = "extension.units.assign_dim_mismatch",
        .stage = "extension"
    };

    ErrorsUnitsDiagnosticDetail detail;
    assert(errors_units_detail_for_diagnostic(&diag, &detail));
    assert(detail.hasDimensionDetail);
    assert(strcmp(detail.context, "assignment") == 0);
    assert(strcmp(detail.leftLabel, "lhs") == 0);
    assert(strcmp(detail.leftDimText, "m") == 0);
    assert(strcmp(detail.rightLabel, "rhs") == 0);
    assert(strcmp(detail.rightDimText, "s") == 0);
    assert(!detail.hasSymbolUnits);

    FisicsUnitsAttachment units[1];
    memset(units, 0, sizeof(units));
    units[0].symbol_stable_id = 0x1111111111111111ULL;
    units[0].symbol_name = "speed";
    units[0].dim_text = "m/s";
    units[0].resolved = true;
    units[0].unit_symbol = "ft/s";
    units[0].unit_family = "velocity";
    units[0].unit_resolved = true;
    analysis_units_store_upsert("/tmp/errors_units_detail/units.c", units, 1, true);

    memset(&detail, 0, sizeof(detail));
    assert(errors_units_detail_for_diagnostic(&diag, &detail));
    assert(detail.hasSymbolUnits);
    assert(strcmp(detail.symbolStableId, "0x1111111111111111") == 0);
    assert(strcmp(detail.symbolName, "speed") == 0);
    assert(strcmp(detail.unitText, "ft/s") == 0);
    assert(strcmp(detail.unitFamily, "velocity") == 0);

    analysis_units_store_clear();
    diagnostic_context_clear();
    puts("errors_units_detail_test: success");
    return 0;
}
