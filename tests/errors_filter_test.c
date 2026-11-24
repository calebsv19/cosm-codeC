#include "ide/Panes/ToolPanels/Errors/errors_filter.h"
#include "ide/Panes/ToolPanels/Errors/errors_rows.h"

#include <assert.h>
#include <stdio.h>

static void test_visible_rows(void) {
    Diagnostic alpha[] = {
        {.message = "hidden first", .severity = DIAG_SEVERITY_INFO},
        {.message = "Match error", .severity = DIAG_SEVERITY_ERROR},
        {.message = "Match warning", .severity = DIAG_SEVERITY_WARNING},
    };
    Diagnostic beta[] = {
        {.message = "different error", .severity = DIAG_SEVERITY_ERROR},
    };
    Diagnostic gamma[] = {
        {.message = "Match note", .severity = DIAG_SEVERITY_INFO},
        {.message = "Match warning", .severity = DIAG_SEVERITY_WARNING},
    };
    ErrorFileSnapshot files[] = {
        {.path = "a.c", .diags = alpha, .count = 3},
        {.path = "empty.c"},
        {.path = "b.c", .diags = beta, .count = 1},
        {.path = "c.c", .diags = gamma, .count = 2},
    };
    bool collapsed[] = {false, false, false};
    ErrorsRowSource source = {
        .files = files, .fileCount = 4,
        .collapsed = collapsed, .collapseCount = 3,
        .query = "MATCH", .showErrors = true, .showWarnings = true,
    };
    FlatDiagRef rows[8] = {0};
    assert(errors_build_rows(&source, rows, 8) == 5);
    assert(rows[0].isHeader && !rows[0].diag && rows[0].path == files[0].path);
    assert(!rows[1].isHeader && rows[1].diag == &alpha[1] && rows[1].fileIndex == 0);
    assert(!rows[2].isHeader && rows[2].diag == &alpha[2] && rows[2].path == files[0].path);
    assert(rows[3].isHeader && !rows[3].diag && rows[3].fileIndex == 3);
    assert(!rows[4].isHeader && rows[4].diag == &gamma[1] && rows[4].path == files[3].path);

    collapsed[0] = true;
    assert(errors_build_rows(&source, rows, 8) == 3);
    assert(rows[0].isHeader && rows[0].fileIndex == 0);
    assert(rows[1].isHeader && rows[1].fileIndex == 3);
    assert(rows[2].diag == &gamma[1]);

    rows[1].fileIndex = -100;
    assert(errors_build_rows(&source, rows, 1) == 1);
    assert(rows[1].fileIndex == -100);
    assert(errors_build_rows(&source, NULL, 0) == 0);

    source.showWarnings = false;
    assert(errors_build_rows(&source, rows, 8) == 1 && rows[0].isHeader);
    source.showErrors = false;
    assert(errors_build_rows(&source, rows, 8) == 0);
    source.showAll = true;
    assert(errors_build_rows(&source, rows, 8) == 4);
    assert(rows[2].diag == &gamma[0] && rows[3].diag == &gamma[1]);
    puts("errors_filter_test: grouped, collapsed, bounded and borrowed rows verified");
}

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

    test_visible_rows();
    puts("errors_filter_test: success");
    return 0;
}
