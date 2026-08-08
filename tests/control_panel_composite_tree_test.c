#include <assert.h>
#include <string.h>

#include "ide/Panes/ControlPanel/control_panel_composite_tree.h"
#include "ide/Panes/ControlPanel/control_tree_payload.h"
#include "ide/UI/Trees/ui_tree_node.h"

static UITreeNode* section_node(const char* label, const char* family) {
    UITreeNode* node = createTreeNode(label, TREE_NODE_SECTION, NODE_COLOR_SECTION, NULL, NULL);
    assert(node != NULL);

    char stableId[CONTROL_TREE_PAYLOAD_STABLE_ID_MAX];
    assert(control_tree_payload_format_section_id(stableId, sizeof(stableId), family, "project"));
    assert(control_tree_node_set_payload(
        node,
        control_tree_payload_create(CONTROL_TREE_NODE_SECTION, stableId, label, "")));
    return node;
}

static void composite_preserves_stable_child_order(void) {
    UITreeNode* symbols = section_node("Symbols", "symbols");
    UITreeNode* units = section_node("Units", "units");

    UITreeNode* composite = control_panel_build_composite_tree(symbols, true, units, true, "No matches");
    assert(composite != NULL);
    assert(strcmp(composite->label, "Control") == 0);
    assert(composite->isExpanded);
    assert(composite->childCount == 2);
    assert(composite->children[0] == symbols);
    assert(composite->children[1] == units);

    const ControlTreeNodePayload* payload = control_tree_node_payload(composite);
    assert(payload != NULL);
    assert(payload->kind == CONTROL_TREE_NODE_SECTION);
    assert(strcmp(payload->stableId, "section:control:root") == 0);

    freeTreeNodeRecursive(composite);
}

static void single_target_passthrough_keeps_existing_root(void) {
    UITreeNode* symbols = section_node("Symbols", "symbols");

    UITreeNode* visible = control_panel_build_composite_tree(symbols, false, NULL, false, "No matches");
    assert(visible == symbols);

    freeTreeNodeRecursive(visible);
}

static void empty_fallback_has_passive_payloads(void) {
    UITreeNode* empty = control_panel_build_composite_tree(NULL, false, NULL, false, "No control targets enabled");
    assert(empty != NULL);
    assert(strcmp(empty->label, "Control") == 0);
    assert(empty->isExpanded);
    assert(empty->childCount == 1);

    const ControlTreeNodePayload* rootPayload = control_tree_node_payload(empty);
    assert(rootPayload != NULL);
    assert(rootPayload->kind == CONTROL_TREE_NODE_SECTION);
    assert(strcmp(rootPayload->stableId, "section:control:empty") == 0);

    UITreeNode* row = empty->children[0];
    assert(row != NULL);
    assert(strcmp(row->label, "No control targets enabled") == 0);
    const ControlTreeNodePayload* rowPayload = control_tree_node_payload(row);
    assert(rowPayload != NULL);
    assert(rowPayload->kind == CONTROL_TREE_NODE_EMPTY);
    assert(strcmp(rowPayload->stableId, "empty:control:No control targets enabled") == 0);

    ControlTreeActivationTarget target;
    assert(!control_tree_node_activation_target(row, &target));

    freeTreeNodeRecursive(empty);
}

int main(void) {
    composite_preserves_stable_child_order();
    single_target_passthrough_keeps_existing_root();
    empty_fallback_has_passive_payloads();
    return 0;
}
