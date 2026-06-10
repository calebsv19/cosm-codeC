#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "Compiler/diagnostics.h"
#include "core/Analysis/analysis_store.h"
#include "core/Diagnostics/diagnostics_engine.h"

static void assert_metadata_diag(const Diagnostic* d) {
    assert(d);
    assert(strcmp(d->filePath, "/tmp/analysis_store_diagnostics_metadata/src/units.c") == 0);
    assert(d->line == 7);
    assert(d->column == 3);
    assert(d->length == 5);
    assert(d->severity == DIAG_SEVERITY_ERROR);
    assert(d->category == DIAG_CATEGORY_EXTENSION);
    assert(d->codeId == FISICS_DIAG_CODE_EXTENSION_UNITS_ASSIGN_DIM_MISMATCH);
    assert(d->message && strcmp(d->message, "assignment dimension mismatch") == 0);
    assert(d->hint && strcmp(d->hint, "convert to compatible units") == 0);
    assert(d->codeName && strcmp(d->codeName, "extension.units.assign_dim_mismatch") == 0);
    assert(d->stage && strcmp(d->stage, "extension") == 0);
}

static void seed_metadata_diag(const char* file_path) {
    FisicsDiagnostic d = {0};
    d.file_path = file_path;
    d.line = 7;
    d.column = 3;
    d.length = 5;
    d.kind = DIAG_ERROR;
    d.code = FISICS_DIAG_CODE_EXTENSION_UNITS_ASSIGN_DIM_MISMATCH;
    d.severity_id = FISICS_DIAG_SEVERITY_ERROR;
    d.category_id = FISICS_DIAG_CATEGORY_EXTENSION;
    d.code_id = FISICS_DIAG_CODE_EXTENSION_UNITS_ASSIGN_DIM_MISMATCH;
    d.message = "assignment dimension mismatch";
    d.hint = "convert to compatible units";
    analysis_store_upsert(file_path, &d, 1u);
}

int main(void) {
    const char* workspace_root = "/tmp/analysis_store_diagnostics_metadata";
    const char* file_path = "/tmp/analysis_store_diagnostics_metadata/src/units.c";

    mkdir(workspace_root, 0755);

    analysis_store_clear();
    clearDiagnostics();

    seed_metadata_diag(file_path);
    assert(analysis_store_file_count() == 1u);
    const AnalysisFileDiagnostics* file = analysis_store_file_at(0u);
    assert(file);
    assert(file->count == 1);
    assert_metadata_diag(&file->diags[0]);

    analysis_store_flatten_to_engine();
    assert(getDiagnosticCount() == 1);
    assert_metadata_diag(getDiagnosticAt(0));

    analysis_store_save(workspace_root);
    analysis_store_clear();
    clearDiagnostics();
    analysis_store_load(workspace_root);

    assert(analysis_store_file_count() == 1u);
    file = analysis_store_file_at(0u);
    assert(file);
    assert(file->count == 1);
    assert_metadata_diag(&file->diags[0]);

    analysis_store_flatten_to_engine();
    assert(getDiagnosticCount() == 1);
    assert_metadata_diag(getDiagnosticAt(0));

    analysis_store_clear();
    clearDiagnostics();
    puts("analysis_store_diagnostics_metadata_test: success");
    return 0;
}
