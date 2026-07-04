#include "core/Analysis/analysis_units_store.h"

#include <stdio.h>
#include <string.h>

#include "test_fixture_utils.h"

static int expect(int condition, const char* message) {
    if (!condition) {
        fprintf(stderr, "%s\n", message);
        return 1;
    }
    return 0;
}

int main(void) {
    char root[256];
    if (expect(ide_test_prepare_workspace(root, sizeof(root), "ide_analysis_units_store_test"),
               "expected workspace setup")) return 1;
    analysis_units_store_clear();

    FisicsUnitsAttachment units[2];
    memset(units, 0, sizeof(units));
    units[0].symbol_stable_id = 0x1111111111111111ULL;
    units[0].has_symbol_stable_id = true;
    units[0].symbol_name = "speed";
    units[0].source_file_path = "/tmp/project/src/a.c";
    units[0].start_line = 12;
    units[0].start_col = 5;
    units[0].end_line = 12;
    units[0].end_col = 10;
    units[0].dim_text = "m/s";
    units[0].dim[0] = 1;
    units[0].dim[2] = -1;
    units[0].resolved = true;
    units[0].unit_source_text = "feet_per_second";
    units[0].unit_name = "foot_per_second";
    units[0].unit_symbol = "ft/s";
    units[0].unit_family = "velocity";
    units[0].unit_resolved = true;

    units[1].symbol_stable_id = 0x2222222222222222ULL;
    units[1].has_symbol_stable_id = true;
    units[1].symbol_name = "count";
    units[1].dim_text = "1";
    units[1].resolved = true;

    analysis_units_store_upsert("/tmp/project/src/a.c", units, 2, true);
    if (expect(analysis_units_store_file_count() == 1, "expected one units file entry")) return 1;

    const AnalysisUnitsAttachment* speed = analysis_units_store_find_by_symbol_id(0x1111111111111111ULL);
    if (expect(speed != NULL, "expected speed units attachment")) return 1;
    if (expect(speed->dim[0] == 1 && speed->dim[2] == -1, "expected speed dimensions")) return 1;
    if (expect(speed->has_symbol_stable_id, "expected speed stable id presence")) return 1;
    if (expect(speed->source_file_path && strcmp(speed->source_file_path, "/tmp/project/src/a.c") == 0, "expected source path")) return 1;
    if (expect(speed->start_line == 12 && speed->start_col == 5, "expected source location")) return 1;
    if (expect(speed->has_concrete_unit, "expected concrete unit fields")) return 1;
    if (expect(speed->unit_symbol && strcmp(speed->unit_symbol, "ft/s") == 0, "expected unit symbol")) return 1;

    analysis_units_store_save(root);
    analysis_units_store_clear();
    if (expect(analysis_units_store_file_count() == 0, "expected clear to empty store")) return 1;
    analysis_units_store_load(root);
    speed = analysis_units_store_find_by_symbol_id(0x1111111111111111ULL);
    if (expect(speed != NULL, "expected persisted speed units attachment")) return 1;
    if (expect(speed->has_symbol_stable_id, "expected persisted stable id presence")) return 1;
    if (expect(speed->source_file_path && strcmp(speed->source_file_path, "/tmp/project/src/a.c") == 0, "expected persisted source path")) return 1;
    if (expect(speed->start_line == 12 && speed->start_col == 5, "expected persisted source location")) return 1;
    if (expect(speed->has_concrete_unit, "expected persisted concrete unit fields")) return 1;
    if (expect(speed->unit_name && strcmp(speed->unit_name, "foot_per_second") == 0, "expected persisted unit name")) return 1;

    analysis_units_store_upsert("/tmp/project/src/a.c", units, 1, false);
    speed = analysis_units_store_find_by_symbol_id(0x1111111111111111ULL);
    if (expect(speed != NULL, "expected speed units attachment after concrete-disabled upsert")) return 1;
    if (expect(!speed->has_concrete_unit, "expected concrete fields to be gated off")) return 1;
    if (expect(speed->unit_symbol == NULL, "expected no concrete unit symbol when gated off")) return 1;

    analysis_units_store_upsert("/tmp/project/src/a.c", NULL, 0, false);
    if (expect(analysis_units_store_find_by_symbol_id(0x1111111111111111ULL) == NULL, "expected empty upsert to clear stale units")) return 1;

    analysis_units_store_upsert("/tmp/project/src/a.c", units, 1, true);
    uint64_t before_remove_stamp = analysis_units_store_combined_stamp();
    analysis_units_store_remove("/tmp/project/src/a.c");
    uint64_t after_remove_stamp = analysis_units_store_combined_stamp();
    if (expect(analysis_units_store_file_count() == 0, "expected remove to clear units file entry")) return 1;
    if (expect(analysis_units_store_find_by_symbol_id(0x1111111111111111ULL) == NULL, "expected remove to clear units attachment lookup")) return 1;
    if (expect(after_remove_stamp != before_remove_stamp, "expected remove to advance units store stamp")) return 1;

    analysis_units_store_clear();
    printf("analysis_units_store_test: ok\n");
    return 0;
}
