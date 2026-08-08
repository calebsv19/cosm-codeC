#include "ide/Panes/ControlPanel/control_panel_composite_tree.h"

#include "ide/Panes/ControlPanel/control_tree_payload.h"
#include "ide/UI/Trees/ui_tree_node.h"

static bool attach_section_payload(UITreeNode* node,
                                   const char* family,
                                   const char* bucket,
                                   const char* label) {
    char stableId[CONTROL_TREE_PAYLOAD_STABLE_ID_MAX];
    if (!control_tree_payload_format_section_id(stableId, sizeof(stableId), family, bucket)) {
        return false;
    }
    return control_tree_node_set_payload(
        node,
        control_tree_payload_create(CONTROL_TREE_NODE_SECTION, stableId, label, ""));
}

static bool attach_empty_payload(UITreeNode* node, const char* family, const char* message) {
    char stableId[CONTROL_TREE_PAYLOAD_STABLE_ID_MAX];
    if (!control_tree_payload_format_empty_id(stableId, sizeof(stableId), family, message)) {
        return false;
    }
    return control_tree_node_set_payload(
        node,
        control_tree_payload_create(CONTROL_TREE_NODE_EMPTY, stableId, message, ""));
}

UITreeNode* control_panel_build_empty_search_tree(const char* message) {
    const char* effectiveMessage = (message && message[0]) ? message : "No matches";
    UITreeNode* root = createTreeNode("Control", TREE_NODE_SECTION, NODE_COLOR_SECTION, NULL, NULL);
    if (!root) return NULL;
    root->isExpanded = true;
    if (!attach_section_payload(root, "control", "empty", "Control")) {
        freeTreeNodeRecursive(root);
        return NULL;
    }

    UITreeNode* line = createTreeNode(effectiveMessage, TREE_NODE_FILE, NODE_COLOR_DEFAULT, NULL, NULL);
    if (!line || !attach_empty_payload(line, "control", effectiveMessage)) {
        if (line) freeTreeNodeRecursive(line);
        freeTreeNodeRecursive(root);
        return NULL;
    }
    addChildNode(root, line);
    return root;
}

UITreeNode* control_panel_build_composite_tree(UITreeNode* symbolsTree,
                                               bool symbolsTreeIsOwned,
                                               UITreeNode* unitsTree,
                                               bool unitsTreeIsOwned,
                                               const char* emptyMessage) {
    int childCount = (symbolsTree ? 1 : 0) + (unitsTree ? 1 : 0);
    if (childCount == 0) {
        return control_panel_build_empty_search_tree(emptyMessage ? emptyMessage : "No matches");
    }
    if (childCount == 1) {
        (void)symbolsTreeIsOwned;
        (void)unitsTreeIsOwned;
        return symbolsTree ? symbolsTree : unitsTree;
    }

    UITreeNode* root = createTreeNode("Control", TREE_NODE_SECTION, NODE_COLOR_SECTION, NULL, NULL);
    if (!root || !attach_section_payload(root, "control", "root", "Control")) {
        if (root) freeTreeNodeRecursive(root);
        if (symbolsTreeIsOwned && symbolsTree) freeTreeNodeRecursive(symbolsTree);
        if (unitsTreeIsOwned && unitsTree) freeTreeNodeRecursive(unitsTree);
        return control_panel_build_empty_search_tree(emptyMessage ? emptyMessage : "No matches");
    }

    root->isExpanded = true;
    addChildNode(root, symbolsTree);
    addChildNode(root, unitsTree);
    return root;
}
