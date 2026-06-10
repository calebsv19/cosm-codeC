#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "core/Analysis/include_graph.h"

static void test_snapshot_and_stamp(void) {
    const char* root = "/tmp/ide_include_graph_snapshot";
    mkdir(root, 0755);

    include_graph_clear();
    uint64_t initial = include_graph_combined_stamp();

    FisicsInclude includes[2] = {0};
    includes[0].resolved_path = "/tmp/ide_include_graph_snapshot/include/a.h";
    includes[0].origin = FISICS_INCLUDE_PROJECT;
    includes[1].resolved_path = "/usr/include/stdio.h";
    includes[1].origin = FISICS_INCLUDE_SYSTEM_ORIGIN;

    include_graph_replace_from_result("/tmp/ide_include_graph_snapshot/src/main.c",
                                      includes,
                                      2,
                                      root);
    assert(include_graph_combined_stamp() == initial + 1u);

    include_graph_lock();
    assert(include_graph_entry_count() == 1u);
    IncludeGraphEntryView entry = include_graph_entry_at(0);
    assert(entry.source_path);
    assert(strcmp(entry.source_path, "/tmp/ide_include_graph_snapshot/src/main.c") == 0);
    assert(entry.dep_count == 1u);
    assert(strcmp(entry.deps[0], "/tmp/ide_include_graph_snapshot/include/a.h") == 0);
    include_graph_unlock();

    char** dependents = NULL;
    size_t dependentCount =
        include_graph_collect_dependents("/tmp/ide_include_graph_snapshot/include/a.h",
                                         &dependents);
    assert(dependentCount == 1u);
    assert(strcmp(dependents[0], "/tmp/ide_include_graph_snapshot/src/main.c") == 0);
    include_graph_free_path_list(dependents, dependentCount);

    include_graph_remove_source("/tmp/ide_include_graph_snapshot/src/main.c");
    assert(include_graph_entry_count() == 0u);
}

int main(void) {
    test_snapshot_and_stamp();
    include_graph_clear();
    puts("include_graph_snapshot_test: success");
    return 0;
}
