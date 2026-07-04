#include "ide/Panes/ControlPanel/control_panel_units_tree.h"

#include "core/Analysis/analysis_symbols_store.h"
#include "core/Analysis/analysis_units_store.h"
#include "ide/Panes/ControlPanel/control_panel.h"
#include "ide/UI/Trees/ui_tree_node.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static UITreeNode* find_label_contains(UITreeNode* node, const char* text) {
    if (!node || !text) return NULL;
    if (node->label && strstr(node->label, text)) return node;
    for (int i = 0; i < node->childCount; ++i) {
        UITreeNode* found = find_label_contains(node->children[i], text);
        if (found) return found;
    }
    return NULL;
}

static int count_label_contains(UITreeNode* node, const char* text) {
    if (!node || !text) return 0;
    int count = (node->label && strstr(node->label, text)) ? 1 : 0;
    for (int i = 0; i < node->childCount; ++i) {
        count += count_label_contains(node->children[i], text);
    }
    return count;
}

int main(void) {
    const char* filePath = "/tmp/control_panel_units_tree/src/units.c";
    analysis_symbols_store_clear();
    analysis_units_store_clear();

    FisicsSymbol symbols[2];
    memset(symbols, 0, sizeof(symbols));
    symbols[0].stable_id = 0x1111111111111111ULL;
    symbols[0].name = "durationSeconds";
    symbols[0].file_path = filePath;
    symbols[0].kind = FISICS_SYMBOL_VARIABLE;
    symbols[0].start_line = 12;
    symbols[0].start_col = 5;
    symbols[1].stable_id = 0x2222222222222222ULL;
    symbols[1].name = "speed";
    symbols[1].file_path = filePath;
    symbols[1].kind = FISICS_SYMBOL_VARIABLE;
    symbols[1].start_line = 18;
    symbols[1].start_col = 9;
    analysis_symbols_store_upsert(filePath, symbols, 2);

    FisicsUnitsAttachment units[2];
    memset(units, 0, sizeof(units));
    units[0].symbol_stable_id = symbols[0].stable_id;
    units[0].has_symbol_stable_id = true;
    units[0].symbol_name = "durationSeconds";
    units[0].source_file_path = filePath;
    units[0].start_line = 12;
    units[0].start_col = 5;
    units[0].dim_text = "s";
    units[0].resolved = true;
    units[0].unit_symbol = "s";
    units[0].unit_name = "second";
    units[0].unit_family = "time";
    units[0].unit_resolved = true;
    units[1].symbol_stable_id = symbols[1].stable_id;
    units[1].has_symbol_stable_id = true;
    units[1].symbol_name = "speed";
    units[1].source_file_path = filePath;
    units[1].start_line = 18;
    units[1].start_col = 9;
    units[1].dim_text = "m/s";
    units[1].resolved = true;
    units[1].unit_symbol = "m/s";
    units[1].unit_name = "meter_per_second";
    units[1].unit_family = "velocity";
    units[1].unit_resolved = true;
    analysis_units_store_upsert(filePath, units, 2, true);
    analysis_units_store_upsert("/tmp/control_panel_units_tree/src/empty.c", NULL, 0, false);

    UITreeNode* tree = control_panel_build_units_tree(NULL, filePath);
    assert(tree != NULL);
    assert(find_label_contains(tree, "durationSeconds"));
    assert(find_label_contains(tree, "empty.c") == NULL);
    UITreeNode* speedNode = find_label_contains(tree, "speed");
    assert(speedNode != NULL);
    const FisicsSymbol* speedSym = (const FisicsSymbol*)speedNode->userData;
    assert(speedSym != NULL);
    assert(speedSym->stable_id == symbols[1].stable_id);
    assert(speedSym->start_line == 18);
    char focusQuery[64];
    assert(control_panel_units_tree_node_focus_query(speedNode, focusQuery, sizeof(focusQuery)));
    assert(strcmp(focusQuery, "speed") == 0);
    assert(!control_panel_units_tree_node_focus_query(tree, focusQuery, sizeof(focusQuery)));

    const AnalysisUnitsAttachment* durationUnits =
        analysis_units_store_find_by_symbol_id(symbols[0].stable_id);
    const AnalysisUnitsAttachment* speedUnits =
        analysis_units_store_find_by_symbol_id(symbols[1].stable_id);
    assert(durationUnits != NULL);
    assert(speedUnits != NULL);
    assert(control_panel_units_query_matches(durationUnits, "time"));
    assert(control_panel_units_query_matches(durationUnits, "second"));
    assert(control_panel_units_query_matches(speedUnits, "m/s"));

    UITreeNode* filtered = control_panel_clone_units_tree_filtered(tree, "velocity", SYMBOL_FILTER_SCOPE_ACTIVE, 0u);
    assert(filtered != NULL);
    UITreeNode* filteredSpeedNode = find_label_contains(filtered, "speed");
    assert(filteredSpeedNode != NULL);
    const FisicsSymbol* filteredSpeedSym = (const FisicsSymbol*)filteredSpeedNode->userData;
    assert(filteredSpeedSym != NULL);
    assert(filteredSpeedSym != speedSym);
    assert(filteredSpeedSym->stable_id == symbols[1].stable_id);
    assert(count_label_contains(filtered, "durationSeconds") == 0);
    assert(find_label_contains(filtered, "empty.c") == NULL);

    UITreeNode* projectOnly = control_panel_clone_units_tree_filtered(tree, "", SYMBOL_FILTER_SCOPE_PROJECT, 0u);
    assert(projectOnly != NULL);
    assert(find_label_contains(projectOnly, "Project Files"));
    assert(find_label_contains(projectOnly, "durationSeconds"));
    assert(find_label_contains(projectOnly, "empty.c") == NULL);
    assert(count_label_contains(projectOnly, "No units") == 0);

    UITreeNode* speedOnly = control_panel_clone_units_tree_filtered(tree,
                                                                    "",
                                                                    SYMBOL_FILTER_SCOPE_ACTIVE,
                                                                    CONTROL_UNIT_DIM_SPEED);
    assert(speedOnly != NULL);
    assert(find_label_contains(speedOnly, "speed"));
    assert(count_label_contains(speedOnly, "durationSeconds") == 0);
    assert(control_panel_units_dimension_matches(speedUnits, CONTROL_UNIT_DIM_SPEED));
    assert(!control_panel_units_dimension_matches(speedUnits, CONTROL_UNIT_DIM_TIME));
    assert(control_panel_units_dimension_matches(durationUnits, CONTROL_UNIT_DIM_TIME));

    FisicsUnitsAttachment preferredSourceUnit;
    memset(&preferredSourceUnit, 0, sizeof(preferredSourceUnit));
    preferredSourceUnit.symbol_stable_id = symbols[0].stable_id;
    preferredSourceUnit.has_symbol_stable_id = true;
    preferredSourceUnit.symbol_name = "mass_a";
    preferredSourceUnit.source_file_path = "/tmp/control_panel_units_tree/src/main.c";
    preferredSourceUnit.start_line = 31;
    preferredSourceUnit.start_col = 7;
    preferredSourceUnit.dim_text = "kg";
    preferredSourceUnit.resolved = true;
    preferredSourceUnit.unit_symbol = "kg";
    preferredSourceUnit.unit_name = "kilogram";
    preferredSourceUnit.unit_family = "mass";
    preferredSourceUnit.unit_resolved = true;
    analysis_units_store_upsert("/tmp/control_panel_units_tree/src/main.c", &preferredSourceUnit, 1, true);
    UITreeNode* preferredTree = control_panel_build_units_tree(NULL, "/tmp/control_panel_units_tree/src/main.c");
    assert(preferredTree != NULL);
    UITreeNode* preferredNode = find_label_contains(preferredTree, "mass_a");
    assert(preferredNode != NULL);
    const FisicsSymbol* preferredSym = (const FisicsSymbol*)preferredNode->userData;
    assert(preferredSym != NULL);
    assert(preferredSym->stable_id == symbols[0].stable_id);
    assert(preferredSym->file_path && strcmp(preferredSym->file_path, "/tmp/control_panel_units_tree/src/main.c") == 0);
    assert(preferredSym->start_line == 31);
    assert(preferredSym->start_col == 7);
    freeTreeNodeRecursive(preferredTree);

    UITreeNode* nullLabelNode = createTreeNode(NULL, TREE_NODE_FILE, NODE_COLOR_DEFAULT, NULL, NULL);
    assert(nullLabelNode != NULL);
    assert(nullLabelNode->label != NULL);
    assert(nullLabelNode->label[0] == '\0');
    freeTreeNodeRecursive(nullLabelNode);

    freeTreeNodeRecursive(projectOnly);
    freeTreeNodeRecursive(speedOnly);
    freeTreeNodeRecursive(filtered);
    freeTreeNodeRecursive(tree);

    FisicsUnitsAttachment localUnit;
    memset(&localUnit, 0, sizeof(localUnit));
    localUnit.symbol_name = "localMass";
    localUnit.source_file_path = filePath;
    localUnit.start_line = 24;
    localUnit.start_col = 13;
    localUnit.dim_text = "kg";
    localUnit.resolved = true;
    localUnit.unit_symbol = "kg";
    localUnit.unit_name = "kilogram";
    localUnit.unit_family = "mass";
    localUnit.unit_resolved = true;
    analysis_units_store_upsert(filePath, &localUnit, 1, true);
    UITreeNode* localTree = control_panel_build_units_tree(NULL, filePath);
    assert(localTree != NULL);
    UITreeNode* localNode = find_label_contains(localTree, "localMass");
    assert(localNode != NULL);
    const FisicsSymbol* localSym = (const FisicsSymbol*)localNode->userData;
    assert(localSym != NULL);
    assert(localSym->stable_id == 0);
    assert(localSym->file_path && strcmp(localSym->file_path, filePath) == 0);
    assert(localSym->start_line == 24);
    assert(control_panel_units_tree_node_focus_query(localNode, focusQuery, sizeof(focusQuery)));
    assert(strcmp(focusQuery, "localMass") == 0);
    freeTreeNodeRecursive(localTree);

    analysis_units_store_clear();
    analysis_symbols_store_clear();
    printf("control_panel_units_tree_test: ok\n");
    return 0;
}
