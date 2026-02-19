#include "ide/UI/Trees/ui_tree_node.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

UITreeNode* createTreeNode(const char* label, TreeNodeType type, TreeNodeColor color, const char* fullPath, void* 
userData) {
    UITreeNode* node = (UITreeNode*)malloc(sizeof(UITreeNode));
    if (!node) return NULL;

    node->type = type;
    node->color = color;

    node->label = label ? strdup(label) : NULL;
    node->fullPath = fullPath ? strdup(fullPath) : NULL;

    node->isExpanded = true;
    node->depth = 0;
    node->userData = userData;

    node->children = NULL;
    node->childCount = 0;
    node->childCapacity = 0;

    return node;
}

static void update_tree_depth(UITreeNode* node, int depth) {
    if (!node) return;
    node->depth = depth;
    for (int i = 0; i < node->childCount; i++) {
        update_tree_depth(node->children[i], depth + 1);
    }
}

void addChildNode(UITreeNode* parent, UITreeNode* child) {
    if (!parent || !child) return;

    if (parent->childCount >= parent->childCapacity) {
        parent->childCapacity = (parent->childCapacity == 0) ? 4 : parent->childCapacity * 2;
        parent->children = realloc(parent->children, parent->childCapacity * sizeof(UITreeNode*));
        if (!parent->children) {
            fprintf(stderr, "[Tree] Failed to realloc child array\n");
            return;
        }
    }

    update_tree_depth(child, parent->depth + 1);
    parent->children[parent->childCount++] = child;
}

void freeTreeNodeRecursive(UITreeNode* node) {
    if (!node) return;

    for (int i = 0; i < node->childCount; i++) {
        freeTreeNodeRecursive(node->children[i]);
    }

    free(node->children);
    free((char*)node->label);
    free((char*)node->fullPath);
    free(node);
}
