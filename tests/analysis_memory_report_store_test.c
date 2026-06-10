#include "core/Analysis/analysis_memory_report_store.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static int failures = 0;

static void check(int cond, const char* label) {
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", label);
        failures++;
    }
}

int main(void) {
    const char* report =
        "{"
        "\"profile\":\"memory_check_report_v1\","
        "\"schema_version\":1,"
        "\"runtime\":\"fisics_memory_check\","
        "\"trigger\":\"manual\","
        "\"summary\":{"
        "\"active\":1,"
        "\"leaked_bytes\":21,"
        "\"allocs\":1,"
        "\"frees\":0,"
        "\"double_free\":0,"
        "\"unknown_free\":0,"
        "\"tracker_failures\":0"
        "},"
        "\"leaks\":[{"
        "\"size\":21,"
        "\"allocated_at\":{\"file\":\"memory_check_json_report.c\",\"line\":7}"
        "}]"
        "}";

    const char* clean_report =
        "{"
        "\"profile\":\"memory_check_report_v1\","
        "\"schema_version\":1,"
        "\"runtime\":\"fisics_memory_check\","
        "\"trigger\":\"atexit\","
        "\"summary\":{"
        "\"active\":0,"
        "\"leaked_bytes\":0,"
        "\"allocs\":2,"
        "\"frees\":2,"
        "\"double_free\":0,"
        "\"unknown_free\":0,"
        "\"tracker_failures\":0"
        "},"
        "\"leaks\":[]"
        "}";

    const char* wrong_profile =
        "{"
        "\"profile\":\"compiler_diagnostics_v1\","
        "\"schema_version\":1,"
        "\"runtime\":\"fisics_memory_check\","
        "\"trigger\":\"manual\","
        "\"summary\":{},"
        "\"leaks\":[]"
        "}";

    analysis_memory_report_store_clear();
    check(!analysis_memory_report_store_load_json_text("/tmp/not_memory.json", wrong_profile), "reject wrong profile");
    check(analysis_memory_report_store_snapshot_count() == 0, "wrong profile not stored");

    check(analysis_memory_report_store_load_json_text("/tmp/memory_report.json", report), "load report");
    check(analysis_memory_report_store_snapshot_count() == 1, "report count");
    const AnalysisMemoryReportSnapshot* snap = analysis_memory_report_store_snapshot_at(0);
    check(snap && strcmp(snap->profile, "memory_check_report_v1") == 0, "profile");
    check(snap && snap->schema_version == 1, "schema version");
    check(snap && strcmp(snap->runtime, "fisics_memory_check") == 0, "runtime");
    check(snap && strcmp(snap->trigger, "manual") == 0, "trigger");
    check(snap && snap->summary.active == 1, "active summary");
    check(snap && snap->summary.leaked_bytes == 21, "leaked bytes");
    check(snap && snap->summary.allocs == 1 && snap->summary.frees == 0, "alloc/free counts");
    check(snap && snap->leak_count == 1, "leak count");
    check(snap && snap->leaks[0].size == 21, "leak size");
    check(snap && strcmp(snap->leaks[0].file, "memory_check_json_report.c") == 0, "leak file");
    check(snap && snap->leaks[0].line == 7, "leak line");

    check(analysis_memory_report_store_load_json_text("/tmp/memory_report.json", clean_report), "replace report");
    check(analysis_memory_report_store_snapshot_count() == 1, "replace count");
    snap = analysis_memory_report_store_snapshot_at(0);
    check(snap && strcmp(snap->trigger, "atexit") == 0, "replacement trigger");
    check(snap && snap->summary.active == 0 && snap->summary.leaked_bytes == 0, "replacement summary");
    check(snap && snap->leak_count == 0, "replacement leak count");

    const char* workspace = "/tmp/ide_memory_report_store_test";
    mkdir(workspace, 0755);
    analysis_memory_report_store_save(workspace);
    analysis_memory_report_store_clear();
    analysis_memory_report_store_load(workspace);
    check(analysis_memory_report_store_snapshot_count() == 1, "persisted count");
    snap = analysis_memory_report_store_snapshot_at(0);
    check(snap && strcmp(snap->trigger, "atexit") == 0, "persisted trigger");
    check(snap && snap->summary.allocs == 2 && snap->summary.frees == 2, "persisted counts");

    analysis_memory_report_store_clear();
    if (failures) {
        fprintf(stderr, "analysis_memory_report_store_test: %d failure(s)\n", failures);
        return 1;
    }
    printf("analysis_memory_report_store_test: ok\n");
    return 0;
}
