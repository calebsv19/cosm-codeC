#include "ide/Panes/ToolPanels/Errors/errors_filter.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    Diagnostic diag = {
        .filePath = "/workspace/src/math/units.c",
        .line = 42,
        .column = 7,
        .length = 3,
        .message = "dimension mismatch in assignment",
        .hint = "convert the right-hand side",
        .severity = DIAG_SEVERITY_ERROR,
        .category = DIAG_CATEGORY_EXTENSION,
        .codeId = 4103,
        .codeName = "extension.units.assign_dim_mismatch",
        .stage = "extension"
    };

    assert(errors_filter_diagnostic_matches_query(&diag, NULL));
    assert(errors_filter_diagnostic_matches_query(&diag, ""));
    assert(errors_filter_diagnostic_matches_query(&diag, "DIMENSION"));
    assert(errors_filter_diagnostic_matches_query(&diag, "units.c"));
    assert(errors_filter_diagnostic_matches_query(&diag, "extension"));
    assert(errors_filter_diagnostic_matches_query(&diag, "convert"));
    assert(errors_filter_diagnostic_matches_query(&diag, "assign_dim"));
    assert(!errors_filter_diagnostic_matches_query(&diag, "lexer"));
    assert(!errors_filter_diagnostic_matches_query(NULL, "dimension"));

    puts("errors_filter_test: success");
    return 0;
}
