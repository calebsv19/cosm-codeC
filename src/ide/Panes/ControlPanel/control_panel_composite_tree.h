#ifndef CONTROL_PANEL_COMPOSITE_TREE_H
#define CONTROL_PANEL_COMPOSITE_TREE_H

#include <stdbool.h>

struct UITreeNode;

struct UITreeNode* control_panel_build_empty_search_tree(const char* message);
struct UITreeNode* control_panel_build_composite_tree(struct UITreeNode* symbolsTree,
                                                      bool symbolsTreeIsOwned,
                                                      struct UITreeNode* unitsTree,
                                                      bool unitsTreeIsOwned,
                                                      const char* emptyMessage);

#endif
