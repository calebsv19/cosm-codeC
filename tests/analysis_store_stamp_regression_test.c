#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "core/Analysis/analysis_store.h"

int main(void) {
    analysis_store_clear();
    uint64_t initial = analysis_store_combined_stamp();

    FisicsDiagnostic d = {0};
    d.file_path = "/tmp/analysis_store_stamp_regression/a.c";
    d.line = 1;
    d.column = 1;
    d.length = 1;
    d.kind = DIAG_WARNING;
    d.message = "seed warning";

    analysis_store_upsert(d.file_path, &d, 1u);
    uint64_t after_upsert = analysis_store_combined_stamp();
    assert(after_upsert > initial);
    assert(analysis_store_file_count() == 1u);

    analysis_store_remove(d.file_path);
    uint64_t after_remove = analysis_store_combined_stamp();
    assert(after_remove > after_upsert);
    assert(analysis_store_file_count() == 0u);

    // Removing a missing file should not mutate the stamp.
    analysis_store_remove(d.file_path);
    assert(analysis_store_combined_stamp() == after_remove);

    analysis_store_clear();
    printf("analysis_store_stamp_regression_test: success\n");
    return 0;
}
