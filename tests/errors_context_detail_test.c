#include "ide/Panes/ToolPanels/Errors/errors_context_detail.h"
#include "core/Diagnostics/diagnostic_context.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static const char* k_context_json =
    "{"
    "\"profile\":\"fisics_diagnostics_v1\","
    "\"diagnostics\":["
    "{"
    "\"file\":\"/tmp/diag_context/main.c\","
    "\"line\":4,\"column\":12,\"length\":5,"
    "\"code_id\":3000,\"code_name\":\"preprocessor.generic\","
    "\"message\":\"unterminated include\","
    "\"include_stack\":["
    "{\"file\":\"/tmp/diag_context/main.c\",\"line\":4,\"column\":3,\"origin\":\"#include \\\"bad.h\\\"\",\"resolved\":true},"
    "{\"file\":\"/tmp/diag_context/bad.h\",\"line\":1,\"column\":1,\"origin\":\"bad.h\",\"resolved\":true}"
    "],"
    "\"macro_trace\":["
    "{\"macro\":\"NEED2\",\"role\":\"call_site\",\"file\":\"/tmp/diag_context/main.c\",\"line\":4,\"column\":12},"
    "{\"macro\":\"NEED2\",\"role\":\"definition\",\"file\":\"/tmp/diag_context/main.c\",\"line\":1,\"column\":9}"
    "],"
    "\"details\":{"
    "\"context\":\"assignment\","
    "\"expected_dim_text\":\"m\","
    "\"actual_dim_text\":\"s\""
    "}"
    "}"
    "]"
    "}";

int main(void) {
    assert(diagnostic_context_load_json_text(k_context_json));

    Diagnostic diag = {0};
    diag.filePath = "/tmp/diag_context/main.c";
    diag.line = 4;
    diag.column = 12;
    diag.codeId = 3000;
    diag.codeName = "preprocessor.generic";
    diag.message = "unterminated include";

    ErrorsContextDetail detail;
    assert(errors_context_detail_for_diagnostic(&diag, &detail));
    assert(detail.includeCount == 2);
    assert(detail.macroCount == 2);
    assert(detail.hasDetails);
    assert(detail.rowCount >= 6);
    assert(detail.hasNavigationTarget);
    assert(strcmp(detail.targetPath, "/tmp/diag_context/main.c") == 0);
    assert(detail.targetLine == 4);
    assert(detail.targetColumn == 3);
    assert(strstr(detail.rows[0].text, "includes 2"));
    assert(strstr(detail.rows[1].text, "include 1"));

    bool sawMacro = false;
    bool sawDetails = false;
    for (int i = 0; i < detail.rowCount; ++i) {
        if (detail.rows[i].kind == ERRORS_CONTEXT_ROW_MACRO &&
            strstr(detail.rows[i].text, "NEED2")) {
            sawMacro = true;
        }
        if (detail.rows[i].kind == ERRORS_CONTEXT_ROW_DETAILS &&
            strstr(detail.rows[i].text, "expected m actual s")) {
            sawDetails = true;
        }
    }
    assert(sawMacro);
    assert(sawDetails);

    diagnostic_context_clear();
    puts("errors_context_detail_test: ok");
    return 0;
}
