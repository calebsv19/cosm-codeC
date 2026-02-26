#ifndef TREE_SNAPSHOT_H
#define TREE_SNAPSHOT_H

#include "ide/UI/Trees/ui_tree_node.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct TreeSnapshotOptions {
    bool include_root;
    bool include_indent;
} TreeSnapshotOptions;

char* tree_build_visible_text_snapshot(const UITreeNode* root,
                                       const TreeSnapshotOptions* options);

size_t tree_count_visible_nodes(const UITreeNode* root, bool include_root);

#endif /* TREE_SNAPSHOT_H */
