#include "core/Analysis/analysis_units_store.h"
#include "core/Diagnostics/diagnostic_context.h"
#include "core/Diagnostics/diagnostic_explanations.h"
#include "ide/Panes/ToolPanels/Errors/errors_diagnostic_detail.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static const char* k_explanations_json =
    "{\"profile\":\"fisics_diagnostic_explanations\",\"schema_version\":1,"
    "\"diagnostics\":[{"
    "\"code_id\":4103,"
    "\"code_name\":\"extension.units.assign_dim_mismatch\","
    "\"category_id\":8,"
    "\"category_name\":\"extension\","
    "\"stage\":\"extension\","
    "\"description\":\"assignment operands have incompatible dimensions\","
    "\"common_causes\":\"sample causes\","
    "\"next_action\":\"sample action\""
    "}]}";

static const char* k_context_json =
    "{"
    "\"diagnostics\":["
    "{"
    "\"file\":\"/tmp/errors_diagnostic_detail/units.c\","
    "\"line\":8,\"column\":7,\"length\":6,"
    "\"code_id\":4103,\"code_name\":\"extension.units.assign_dim_mismatch\","
    "\"message\":\"assignment dimension mismatch\","
    "\"include_stack\":["
    "{\"file\":\"/tmp/errors_diagnostic_detail/units.c\","
    "\"line\":2,\"column\":1,\"origin\":\"#include \\\"units.h\\\"\",\"resolved\":true}"
    "],"
    "\"macro_trace\":["
    "{\"macro\":\"ASSIGN\",\"role\":\"call_site\","
    "\"file\":\"/tmp/errors_diagnostic_detail/units.c\",\"line\":8,\"column\":7}"
    "],"
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

static bool model_contains(const ErrorsDiagnosticDetailModel* model, const char* text) {
    for (int i = 0; i < model->lineCount; ++i) {
        if (strstr(model->lines[i].text, text)) return true;
    }
    return false;
}

static bool model_contains_kind(const ErrorsDiagnosticDetailModel* model,
                                ErrorsDiagnosticDetailLineKind kind,
                                const char* text) {
    for (int i = 0; i < model->lineCount; ++i) {
        if (model->lines[i].kind == kind && strstr(model->lines[i].text, text)) {
            return true;
        }
    }
    return false;
}

int main(void) {
    diagnostic_context_clear();
    diagnostic_explanations_clear();
    analysis_units_store_clear();
    assert(diagnostic_explanations_load_json_text(k_explanations_json));
    assert(diagnostic_context_load_json_text(k_context_json));

    FisicsUnitsAttachment units[1];
    memset(units, 0, sizeof(units));
    units[0].symbol_stable_id = 0x1111111111111111ULL;
    units[0].symbol_name = "distance";
    units[0].dim_text = "m";
    units[0].resolved = true;
    units[0].unit_symbol = "ft";
    units[0].unit_family = "length";
    units[0].unit_resolved = true;
    analysis_units_store_upsert("/tmp/errors_diagnostic_detail/units.c", units, 1, true);

    Diagnostic diag = {
        .filePath = "/tmp/errors_diagnostic_detail/units.c",
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

    ErrorsDiagnosticDetailModel model;
    assert(errors_diagnostic_detail_build(&diag, "src/units.c", &model));
    assert(model.lineCount >= 8);
    assert(strcmp(model.lines[0].text, "Diagnostic Detail  Error") == 0);
    assert(strcmp(model.lines[1].text, "src/units.c:8:7 span 6") == 0);
    assert(model_contains(&model, "category: extension"));
    assert(model_contains(&model, "extension.units.assign_dim_mismatch (4103)"));
    assert(model_contains_kind(&model,
                               ERRORS_DIAGNOSTIC_DETAIL_LINE_MESSAGE,
                               "assignment dimension mismatch"));
    assert(model_contains_kind(&model,
                               ERRORS_DIAGNOSTIC_DETAIL_LINE_HINT,
                               "convert to compatible units"));
    assert(model_contains_kind(&model,
                               ERRORS_DIAGNOSTIC_DETAIL_LINE_EXPLANATION,
                               "incompatible dimensions"));
    assert(model_contains_kind(&model,
                               ERRORS_DIAGNOSTIC_DETAIL_LINE_UNITS,
                               "symbol distance ft"));
    assert(model_contains_kind(&model,
                               ERRORS_DIAGNOSTIC_DETAIL_LINE_NAV_CONTEXT,
                               "open"));
    assert(model_contains_kind(&model,
                               ERRORS_DIAGNOSTIC_DETAIL_LINE_CONTEXT,
                               "details"));

    analysis_units_store_clear();
    diagnostic_explanations_clear();
    diagnostic_context_clear();
    puts("errors_diagnostic_detail_test: success");
    return 0;
}
