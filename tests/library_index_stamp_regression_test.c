#include "test_tmp_root.h"
#include <assert.h>
#include <stdio.h>

#include "core/Analysis/library_index.h"

static void test_library_index_stamps(void) {
    library_index_reset();
    assert(library_index_combined_stamp() == 0u);
    assert(library_index_published_stamp() == 0u);

    library_index_begin("" IDE_TEST_TMPDIR "/project");
    library_index_add_include("" IDE_TEST_TMPDIR "/project/src/main.c",
                              "stdio.h",
                              "/usr/include/stdio.h",
                              LIB_INCLUDE_KIND_SYSTEM,
                              LIB_BUCKET_SYSTEM,
                              1,
                              1);
    assert(library_index_combined_stamp() == 0u);
    library_index_finalize();
    assert(library_index_combined_stamp() == 1u);

    library_index_mark_published(1u);
    assert(library_index_published_stamp() == 1u);

    library_index_add_include("" IDE_TEST_TMPDIR "/project/src/main.c",
                              "stdlib.h",
                              "/usr/include/stdlib.h",
                              LIB_INCLUDE_KIND_SYSTEM,
                              LIB_BUCKET_SYSTEM,
                              2,
                              1);
    library_index_finalize();
    assert(library_index_combined_stamp() == 2u);
    assert(library_index_published_stamp() == 1u);

    library_index_mark_published(999u);
    assert(library_index_published_stamp() == 2u);

    library_index_remove_source("" IDE_TEST_TMPDIR "/project/src/main.c");
    library_index_finalize();
    assert(library_index_combined_stamp() == 3u);

    library_index_mark_published(0u);
    assert(library_index_published_stamp() == 0u);
}

int main(void) {
    test_library_index_stamps();
    puts("library_index_stamp_regression_test: success");
    return 0;
}
