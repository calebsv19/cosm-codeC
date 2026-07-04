#include "core/Analysis/analysis_incremental_policy.h"

#include <assert.h>
#include <stdio.h>

static AnalysisFileFingerprint make_file(const char* path,
                                         long mtime,
                                         long long size,
                                         uint64_t hash) {
    AnalysisFileFingerprint f = {0};
    f.path = (char*)path;
    f.mtime = mtime;
    f.size = size;
    f.content_hash = hash;
    return f;
}

static void test_saved_file_hint_hash_filter(void) {
    AnalysisFileFingerprint cached_files[] = {
        make_file("/tmp/project/src/a.c", 10, 4, 100),
        make_file("/tmp/project/src/b.c", 11, 4, 200),
    };
    AnalysisFileFingerprint current_files[] = {
        make_file("/tmp/project/src/a.c", 20, 4, 100),
        make_file("/tmp/project/src/b.c", 21, 5, 201),
        make_file("/tmp/project/src/new.c", 22, 3, 300),
    };
    AnalysisSnapshot cached = {0};
    AnalysisSnapshot current = {0};
    cached.files = cached_files;
    cached.file_count = sizeof(cached_files) / sizeof(cached_files[0]);
    current.files = current_files;
    current.file_count = sizeof(current_files) / sizeof(current_files[0]);

    assert(!analysis_incremental_policy_should_analyze_hint(&cached,
                                                            &current,
                                                            "/tmp/project/src/a.c"));
    assert(analysis_incremental_policy_should_analyze_hint(&cached,
                                                           &current,
                                                           "/tmp/project/src/b.c"));
    assert(analysis_incremental_policy_should_analyze_hint(&cached,
                                                           &current,
                                                           "/tmp/project/src/new.c"));
    assert(!analysis_incremental_policy_should_analyze_hint(&cached,
                                                            &current,
                                                            "/tmp/project/docs/readme.md"));
}

static void test_missing_include_graph_header_fallback(void) {
    char* dirty_c[] = {(char*)"/tmp/project/src/a.c"};
    char* dirty_h[] = {(char*)"/tmp/project/include/a.h"};
    char* removed_h[] = {(char*)"/tmp/project/include/old.h"};
    const char* hints_h[] = {"/tmp/project/include/saved.h"};

    assert(!analysis_incremental_policy_requires_full_for_missing_include_graph(
        0, dirty_c, 1, NULL, 0, NULL, 0));
    assert(analysis_incremental_policy_requires_full_for_missing_include_graph(
        0, dirty_h, 1, NULL, 0, NULL, 0));
    assert(analysis_incremental_policy_requires_full_for_missing_include_graph(
        0, NULL, 0, removed_h, 1, NULL, 0));
    assert(analysis_incremental_policy_requires_full_for_missing_include_graph(
        0, NULL, 0, NULL, 0, hints_h, 1));
    assert(!analysis_incremental_policy_requires_full_for_missing_include_graph(
        3, dirty_h, 1, removed_h, 1, hints_h, 1));
}

int main(void) {
    test_saved_file_hint_hash_filter();
    test_missing_include_graph_header_fallback();
    puts("analysis_incremental_policy_test: ok");
    return 0;
}
