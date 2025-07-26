#ifndef TREE_GIT_ADAPTER_H
#define TREE_GIT_ADAPTER_H

#include "ide/UI/Trees/ui_tree_node.h"

// Converts the current `gitFiles[]` to a tree structure for display
UITreeNode* convertGitStatusToTree(void);

// Frees the generated tree
void freeGitTree(UITreeNode* root);

#endif

