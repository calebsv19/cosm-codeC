#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/Diagnostics/diagnostics_engine.h"
#include "test_fixture_utils.h"

static void artifact_path(const char* workspace, char* out, size_t out_cap) {
    snprintf(out, out_cap, "%s/ide_files/analysis_diagnostics.json", workspace);
}

static void test_missing_is_non_noisy(void) {
    char workspace[256];
    DiagnosticIoReport report;
    assert(ide_test_fixture_root(workspace, sizeof(workspace), "diagnostics_artifact_io_missing"));
    assert(ide_test_ensure_dir(workspace));
    initDiagnosticsEngine();
    assert(diagnostics_load_report(workspace, &report));
    assert(report.operation == DIAGNOSTIC_IO_LOAD);
    assert(report.reason == DIAGNOSTIC_IO_REASON_MISSING_OK);
    assert(report.ok);
    assert(!report.noisy_failure);
    assert(getDiagnosticCount() == 0);
}

static void test_save_and_load_report(void) {
    char workspace[256];
    DiagnosticIoReport report;
    assert(ide_test_prepare_workspace(workspace, sizeof(workspace), "diagnostics_artifact_io_roundtrip"));

    initDiagnosticsEngine();
    addDiagnosticWithDetails("/tmp/diagnostics_artifact_io_roundtrip/src/main.c",
                             4,
                             2,
                             3,
                             "example warning",
                             "example hint",
                             DIAG_SEVERITY_WARNING,
                             DIAG_CATEGORY_ANALYSIS,
                             0,
                             "analysis.example",
                             "analysis");

    assert(diagnostics_save_report(workspace, &report));
    assert(report.operation == DIAGNOSTIC_IO_SAVE);
    assert(report.reason == DIAGNOSTIC_IO_REASON_SAVED);
    assert(report.saved_rows == 1);
    assert(report.ok);
    assert(!report.noisy_failure);

    clearDiagnostics();
    assert(diagnostics_load_report(workspace, &report));
    assert(report.operation == DIAGNOSTIC_IO_LOAD);
    assert(report.reason == DIAGNOSTIC_IO_REASON_LOADED);
    assert(report.loaded_rows == 1);
    assert(report.malformed_rows == 0);
    assert(report.ok);
    assert(!report.noisy_failure);
    assert(getDiagnosticCount() == 1);
    const Diagnostic* diag = getDiagnosticAt(0);
    assert(diag);
    assert(strcmp(diag->message, "example warning") == 0);
    assert(strcmp(diag->hint, "example hint") == 0);
}

static void test_invalid_json_report(void) {
    char workspace[256];
    char path[1024];
    DiagnosticIoReport report;
    assert(ide_test_prepare_workspace(workspace, sizeof(workspace), "diagnostics_artifact_io_invalid"));
    artifact_path(workspace, path, sizeof(path));
    assert(ide_test_write_text_file(path, "{not-json"));

    initDiagnosticsEngine();
    assert(!diagnostics_load_report(workspace, &report));
    assert(report.reason == DIAGNOSTIC_IO_REASON_LOAD_INVALID_JSON);
    assert(!report.ok);
    assert(report.noisy_failure);
    assert(getDiagnosticCount() == 0);
}

static void test_invalid_root_report(void) {
    char workspace[256];
    char path[1024];
    DiagnosticIoReport report;
    assert(ide_test_prepare_workspace(workspace, sizeof(workspace), "diagnostics_artifact_io_root"));
    artifact_path(workspace, path, sizeof(path));
    assert(ide_test_write_text_file(path, "{\"diagnostics\":[]}"));

    initDiagnosticsEngine();
    assert(!diagnostics_load_report(workspace, &report));
    assert(report.reason == DIAGNOSTIC_IO_REASON_LOAD_INVALID_ROOT);
    assert(!report.ok);
    assert(report.noisy_failure);
}

static void test_malformed_rows_report(void) {
    char workspace[256];
    char path[1024];
    DiagnosticIoReport report;
    assert(ide_test_prepare_workspace(workspace, sizeof(workspace), "diagnostics_artifact_io_malformed"));
    artifact_path(workspace, path, sizeof(path));
    assert(ide_test_write_text_file(
        path,
        "[{\"file\":\"/tmp/a.c\",\"line\":1,\"col\":2,\"severity\":1,\"message\":\"ok\"},"
        "{\"file\":\"/tmp/b.c\",\"message\":\"missing required fields\"}]"));

    initDiagnosticsEngine();
    assert(diagnostics_load_report(workspace, &report));
    assert(report.reason == DIAGNOSTIC_IO_REASON_LOAD_MALFORMED_ROWS);
    assert(report.ok);
    assert(report.noisy_failure);
    assert(report.loaded_rows == 1);
    assert(report.malformed_rows == 1);
    assert(getDiagnosticCount() == 1);
}

static void test_oversized_report(void) {
    char workspace[256];
    char path[1024];
    DiagnosticIoReport report;
    assert(ide_test_prepare_workspace(workspace, sizeof(workspace), "diagnostics_artifact_io_oversized"));
    artifact_path(workspace, path, sizeof(path));
    assert(ide_test_write_sparse_file(path, (1 << 20) + 1));

    initDiagnosticsEngine();
    assert(!diagnostics_load_report(workspace, &report));
    assert(report.reason == DIAGNOSTIC_IO_REASON_LOAD_OVERSIZED);
    assert(!report.ok);
    assert(report.noisy_failure);
}

int main(void) {
    test_missing_is_non_noisy();
    test_save_and_load_report();
    test_invalid_json_report();
    test_invalid_root_report();
    test_malformed_rows_report();
    test_oversized_report();
    clearDiagnostics();
    puts("diagnostics_artifact_io_test: success");
    return 0;
}
