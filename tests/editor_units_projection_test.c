#include "ide/Panes/Editor/editor_projection.h"

#include "core/Analysis/analysis_symbols_store.h"
#include "core/Analysis/analysis_units_store.h"
#include "ide/Panes/ControlPanel/control_panel.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool g_symbols_enabled = false;
static bool g_units_enabled = true;
static unsigned int g_unit_dimension_mask = 0u;

bool control_panel_target_symbols_enabled(void) {
    return g_symbols_enabled;
}

bool control_panel_target_units_enabled(void) {
    return g_units_enabled;
}

unsigned int control_panel_get_unit_dimension_mask(void) {
    return g_unit_dimension_mask;
}

void control_panel_get_match_button_order(ControlFilterButtonId outOrder[4]) {
    if (!outOrder) return;
    outOrder[0] = CONTROL_FILTER_BTN_MATCH_METHODS;
    outOrder[1] = CONTROL_FILTER_BTN_MATCH_TYPES;
    outOrder[2] = CONTROL_FILTER_BTN_MATCH_VARS;
    outOrder[3] = CONTROL_FILTER_BTN_MATCH_TAGS;
}

void editor_projection_free(SearchProjection* projection) {
    if (!projection) return;
    for (int i = 0; i < projection->lineCount; ++i) {
        free(projection->lines[i]);
    }
    free(projection->lines);
    free(projection->projectedToRealLine);
    free(projection->projectedToRealCol);
    free(projection->realMatchLines);
    free(projection->realMatchKinds);
    memset(projection, 0, sizeof(*projection));
}

int main(void) {
    const char* filePath = "/tmp/editor_units_projection/src/physics.c";
    analysis_symbols_store_clear();
    analysis_units_store_clear();

    char* lines[] = {
        "double unannotated = 1.0;",
        "double velocity_x [[fisics::dim(speed)]] [[fisics::unit(meter_per_second)]] = 0.0;",
        "double dt [[fisics::dim(time)]] [[fisics::unit(second)]] = 0.016;",
        "double mass [[fisics::dim(mass)]] [[fisics::unit(kilogram)]] = 1.0;"
    };
    EditorBuffer buffer = {
        .lines = lines,
        .lineCount = 4,
        .capacity = 4
    };
    OpenFile file;
    memset(&file, 0, sizeof(file));
    file.filePath = (char*)filePath;
    file.buffer = &buffer;
    file.bufferVersion = 1u;

    FisicsUnitsAttachment units[3];
    memset(units, 0, sizeof(units));
    units[0].symbol_name = "velocity_x";
    units[0].source_file_path = filePath;
    units[0].start_line = 2;
    units[0].start_col = 8;
    units[0].dim_text = "m/s";
    units[0].resolved = true;
    units[0].unit_symbol = "m/s";
    units[0].unit_name = "meter_per_second";
    units[0].unit_family = "velocity";
    units[0].unit_resolved = true;
    units[1].symbol_name = "dt";
    units[1].source_file_path = filePath;
    units[1].start_line = 3;
    units[1].start_col = 8;
    units[1].dim_text = "s";
    units[1].resolved = true;
    units[1].unit_symbol = "s";
    units[1].unit_name = "second";
    units[1].unit_family = "time";
    units[1].unit_resolved = true;
    units[2].symbol_name = "mass";
    units[2].source_file_path = filePath;
    units[2].start_line = 4;
    units[2].start_col = 8;
    units[2].dim_text = "kg";
    units[2].resolved = true;
    units[2].unit_symbol = "kg";
    units[2].unit_name = "kilogram";
    units[2].unit_family = "mass";
    units[2].unit_resolved = true;
    analysis_units_store_upsert(filePath, units, 3, true);

    editor_projection_rebuild(&file, "", NULL);
    assert(file.projection.lineCount == 3);
    assert(file.projection.realMatchCount == 3);
    assert(file.projection.projectedToRealLine[0] == 1);
    assert(file.projection.projectedToRealLine[1] == 2);
    assert(file.projection.projectedToRealLine[2] == 3);
    assert(file.projection.realMatchKinds != NULL);
    assert(file.projection.realMatchKinds[0] == EDITOR_PROJECTION_MATCH_UNIT_SPEED);
    assert(file.projection.realMatchKinds[1] == EDITOR_PROJECTION_MATCH_UNIT_TIME);
    assert(file.projection.realMatchKinds[2] == EDITOR_PROJECTION_MATCH_UNIT_MASS);
    editor_projection_free(&file.projection);

    g_unit_dimension_mask = CONTROL_UNIT_DIM_TIME;
    editor_projection_rebuild(&file, "", NULL);
    assert(file.projection.lineCount == 1);
    assert(file.projection.realMatchCount == 1);
    assert(file.projection.projectedToRealLine[0] == 2);
    assert(file.projection.realMatchKinds != NULL);
    assert(file.projection.realMatchKinds[0] == EDITOR_PROJECTION_MATCH_UNIT_TIME);
    assert(strstr(file.projection.lines[0], "dt") != NULL);
    editor_projection_free(&file.projection);

    g_unit_dimension_mask = 0u;
    editor_projection_rebuild(&file, "meter_per_second", NULL);
    assert(file.projection.lineCount == 1);
    assert(file.projection.realMatchCount == 1);
    assert(file.projection.projectedToRealLine[0] == 1);
    editor_projection_free(&file.projection);

    analysis_units_store_clear();
    analysis_symbols_store_clear();
    printf("editor_units_projection_test: ok\n");
    return 0;
}
