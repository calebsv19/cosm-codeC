#include "ide/Panes/ControlPanel/control_tree_payload.h"

#include "ide/UI/Trees/ui_tree_node.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    char stableId[CONTROL_TREE_PAYLOAD_STABLE_ID_MAX];
    assert(control_tree_payload_format_section_id(stableId, sizeof(stableId), "symbols", "project"));
    assert(strcmp(stableId, "section:symbols:project") == 0);

    assert(control_tree_payload_format_file_id(stableId, sizeof(stableId), "units", "src/main.c"));
    assert(strcmp(stableId, "file:units:src/main.c") == 0);

    assert(control_tree_payload_format_symbol_id(stableId,
                                                 sizeof(stableId),
                                                 "src/main.c",
                                                 12,
                                                 5,
                                                 "fn",
                                                 "main"));
    assert(strcmp(stableId, "symbol:src/main.c:12:5:fn:main") == 0);

    assert(control_tree_payload_format_unit_id(stableId,
                                               sizeof(stableId),
                                               "src/main.c",
                                               9,
                                               5,
                                               "mass_a",
                                               "kg"));
    assert(strcmp(stableId, "unit:src/main.c:9:5:mass_a:kg") == 0);

    ControlTreeNodePayload* payload = control_tree_payload_create(CONTROL_TREE_NODE_UNIT,
                                                                 stableId,
                                                                 "mass_a : kg",
                                                                 "src/main.c");
    assert(payload != NULL);
    assert(payload->kind == CONTROL_TREE_NODE_UNIT);
    assert(strcmp(payload->stableId, "unit:src/main.c:9:5:mass_a:kg") == 0);
    assert(control_tree_payload_set_target(payload, "src/main.c", 9, 5));
    assert(payload->target.hasTarget);
    assert(strcmp(payload->target.path, "src/main.c") == 0);
    assert(payload->target.line == 9);
    assert(payload->target.column == 5);
    assert(control_tree_payload_set_marker_query(payload, "mass_a"));
    assert(payload->focusMarkerAfterOpen);
    assert(strcmp(payload->markerQuery, "mass_a") == 0);

    ControlTreeNodePayload* clone = control_tree_payload_clone(payload);
    assert(clone != NULL);
    assert(clone != payload);
    assert(clone->kind == payload->kind);
    assert(strcmp(clone->stableId, payload->stableId) == 0);
    assert(strcmp(clone->target.path, payload->target.path) == 0);
    assert(strcmp(clone->markerQuery, payload->markerQuery) == 0);

    UITreeNode* node = createTreeNode("mass_a : kg", TREE_NODE_FILE, NODE_COLOR_DEFAULT, "src/main.c", NULL);
    assert(node != NULL);
    uintptr_t noIdentity = control_tree_node_stable_identity(node);
    assert(noIdentity == 0u);
    assert(control_tree_node_set_payload(node, clone));
    assert(control_tree_node_payload(node) == clone);
    assert(control_tree_node_payload_mutable(node) == clone);
    assert(strcmp(control_tree_node_kind_name(control_tree_node_payload(node)->kind), "unit") == 0);
    uintptr_t identity = control_tree_node_stable_identity(node);
    assert(identity != 0u);
    assert(identity == control_tree_node_stable_identity(node));

    ControlTreeActivationTarget target;
    assert(control_tree_node_activation_target(node, &target));
    assert(target.hasTarget);
    assert(strcmp(target.path, "src/main.c") == 0);
    assert(target.line == 9);
    assert(target.column == 5);

    char markerQuery[64];
    assert(control_tree_node_marker_query(node, markerQuery, sizeof(markerQuery)));
    assert(strcmp(markerQuery, "mass_a") == 0);

    UITreeNode* sectionNode = createTreeNode("Project Files", TREE_NODE_SECTION, NODE_COLOR_SECTION, NULL, NULL);
    assert(sectionNode != NULL);
    char sectionId[CONTROL_TREE_PAYLOAD_STABLE_ID_MAX];
    assert(control_tree_payload_format_section_id(sectionId, sizeof(sectionId), "symbols", "project"));
    ControlTreeNodePayload* sectionPayload = control_tree_payload_create(CONTROL_TREE_NODE_SECTION,
                                                                         sectionId,
                                                                         "Project Files",
                                                                         NULL);
    assert(sectionPayload != NULL);
    assert(control_tree_payload_set_target(sectionPayload, "src/main.c", 1, 1));
    assert(control_tree_node_set_payload(sectionNode, sectionPayload));
    assert(!control_tree_node_activation_target(sectionNode, &target));

    UITreeNode* emptyNode = createTreeNode("No units", TREE_NODE_FILE, NODE_COLOR_DEFAULT, "src/main.c", NULL);
    assert(emptyNode != NULL);
    char emptyId[CONTROL_TREE_PAYLOAD_STABLE_ID_MAX];
    assert(control_tree_payload_format_empty_id(emptyId, sizeof(emptyId), "units", "No units"));
    ControlTreeNodePayload* emptyPayload = control_tree_payload_create(CONTROL_TREE_NODE_EMPTY,
                                                                       emptyId,
                                                                       "No units",
                                                                       NULL);
    assert(emptyPayload != NULL);
    assert(control_tree_node_set_payload(emptyNode, emptyPayload));
    assert(control_tree_node_stable_identity(emptyNode) != 0u);
    assert(!control_tree_node_activation_target(emptyNode, &target));
    assert(!control_tree_node_marker_query(emptyNode, markerQuery, sizeof(markerQuery)));

    freeTreeNodeRecursive(node);
    freeTreeNodeRecursive(sectionNode);
    freeTreeNodeRecursive(emptyNode);
    control_tree_payload_free(payload);

    printf("control_tree_payload_test: ok\n");
    return 0;
}
