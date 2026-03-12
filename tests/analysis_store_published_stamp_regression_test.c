#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "core/Analysis/analysis_store.h"

int main(void) {
    analysis_store_clear();
    assert(analysis_store_published_stamp() == 0u);

    FisicsDiagnostic d = {0};
    d.file_path = "/tmp/analysis_store_published_stamp_regression/a.c";
    d.line = 1;
    d.column = 1;
    d.length = 1;
    d.kind = DIAG_WARNING;
    d.message = "published-stamp";

    analysis_store_upsert(d.file_path, &d, 1u);
    uint64_t combined_after_upsert = analysis_store_combined_stamp();
    assert(combined_after_upsert > 0u);
    // Raw store mutation alone should not move the published watermark.
    assert(analysis_store_published_stamp() == 0u);

    analysis_store_mark_published(combined_after_upsert);
    assert(analysis_store_published_stamp() == combined_after_upsert);

    analysis_store_remove(d.file_path);
    uint64_t combined_after_remove = analysis_store_combined_stamp();
    assert(combined_after_remove > combined_after_upsert);
    // Published watermark remains at last dispatched value until explicitly updated.
    assert(analysis_store_published_stamp() == combined_after_upsert);

    analysis_store_mark_published(combined_after_remove);
    assert(analysis_store_published_stamp() == combined_after_remove);

    analysis_store_clear();
    assert(analysis_store_published_stamp() == 0u);

    printf("analysis_store_published_stamp_regression_test: success\n");
    return 0;
}
