#include "core/Analysis/analysis_build_graph_store.h"

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
    const char* source_graph =
        "{"
        "\"schema\":\"fisiCs.build_graph\","
        "\"version\":0,"
        "\"project_root\":\"/tmp/project\","
        "\"mode\":\"source\","
        "\"partial\":true,"
        "\"fatal\":false,"
        "\"diagnostic_summary\":{\"available\":true,\"total\":2,\"errors\":1,\"warnings\":1,\"notes\":0,\"partial\":true,\"fatal\":false},"
        "\"translation_units\":[{"
        "\"id\":\"tu0\","
        "\"source\":\"/tmp/project/src/a.c\","
        "\"object\":\"/tmp/project/build/a.o\","
        "\"status\":\"partial\","
        "\"diagnostic_summary\":{\"available\":true,\"total\":2,\"errors\":1,\"warnings\":1,\"notes\":0,\"partial\":true,\"fatal\":false}"
        "}],"
        "\"plan\":{\"schema\":\"fisiCs.build_plan\",\"version\":0,\"dry_run\":false,\"actions\":[]}"
        "}";

    const char* dry_run_graph =
        "{"
        "\"schema\":\"fisiCs.build_graph\","
        "\"version\":0,"
        "\"project_root\":\"/tmp/project\","
        "\"mode\":\"dry_run\","
        "\"partial\":false,"
        "\"fatal\":false,"
        "\"diagnostic_summary\":{\"available\":false,\"total\":0,\"errors\":0,\"warnings\":0,\"notes\":0,\"partial\":false,\"fatal\":false},"
        "\"translation_units\":[{"
        "\"id\":\"tu0\","
        "\"source\":\"/tmp/project/src/b.c\","
        "\"object\":\"/tmp/project/build/b.o\","
        "\"status\":\"ok\","
        "\"diagnostic_summary\":{\"available\":false,\"total\":0,\"errors\":0,\"warnings\":0,\"notes\":0,\"partial\":false,\"fatal\":false}"
        "}],"
        "\"plan\":{\"schema\":\"fisiCs.build_plan\",\"version\":0,\"dry_run\":true,\"actions\":["
        "{"
        "\"id\":\"compile0\","
        "\"kind\":\"compile\","
        "\"status\":\"planned\","
        "\"will_execute\":false,"
        "\"source\":\"/tmp/project/src/b.c\","
        "\"object\":\"/tmp/project/build/b.o\","
        "\"diagnostic_summary\":{\"available\":false,\"total\":0,\"errors\":0,\"warnings\":0,\"notes\":0,\"partial\":false,\"fatal\":false}"
        "},"
        "{"
        "\"id\":\"link0\","
        "\"kind\":\"link\","
        "\"status\":\"planned\","
        "\"will_execute\":false,"
        "\"output\":\"/tmp/project/build/app\","
        "\"diagnostic_summary\":{\"available\":false,\"total\":0,\"errors\":0,\"warnings\":0,\"notes\":0,\"partial\":false,\"fatal\":false}"
        "}"
        "]}"
        "}";

    analysis_build_graph_store_clear();
    check(analysis_build_graph_store_load_json_text("/tmp/source_graph.json", source_graph), "load source graph");
    check(analysis_build_graph_store_snapshot_count() == 1, "source graph count");
    const AnalysisBuildGraphSnapshot* source = analysis_build_graph_store_snapshot_at(0);
    check(source && strcmp(source->mode, "source") == 0, "source mode");
    check(source && source->partial && !source->fatal, "source status");
    check(source && source->diagnostic_summary.available, "source summary available");
    check(source && source->diagnostic_summary.errors == 1, "source summary errors");
    check(source && source->translation_unit_count == 1, "source tu count");
    check(source && strcmp(source->translation_units[0].status, "partial") == 0, "source tu status");
    check(source && source->translation_units[0].diagnostic_summary.available, "source tu summary available");

    check(analysis_build_graph_store_load_json_text("/tmp/dry_run_graph.json", dry_run_graph), "load dry-run graph");
    check(analysis_build_graph_store_snapshot_count() == 2, "two graph count");
    const AnalysisBuildGraphSnapshot* dry = analysis_build_graph_store_snapshot_at(0);
    check(dry && strcmp(dry->mode, "dry_run") == 0, "dry-run newest");
    check(dry && dry->action_count == 2, "dry-run action count");
    check(dry && strcmp(dry->actions[0].kind, "compile") == 0, "compile action kind");
    check(dry && !dry->actions[0].will_execute, "compile action planned");
    check(dry && !dry->actions[0].diagnostic_summary.available, "compile action summary unavailable");
    check(dry && strcmp(dry->actions[1].kind, "link") == 0, "link action kind");
    check(dry && strcmp(dry->actions[1].output, "/tmp/project/build/app") == 0, "link action output");

    const char* workspace = "/tmp/ide_build_graph_store_test";
    mkdir(workspace, 0755);
    analysis_build_graph_store_save(workspace);
    analysis_build_graph_store_clear();
    analysis_build_graph_store_load(workspace);
    check(analysis_build_graph_store_snapshot_count() == 2, "persisted graph count");
    dry = analysis_build_graph_store_snapshot_at(0);
    check(dry && dry->action_count == 2, "persisted dry-run actions");
    check(dry && strcmp(dry->actions[0].status, "planned") == 0, "persisted action status");

    analysis_build_graph_store_clear();
    if (failures) {
        fprintf(stderr, "analysis_build_graph_store_test: %d failure(s)\n", failures);
        return 1;
    }
    printf("analysis_build_graph_store_test: ok\n");
    return 0;
}
