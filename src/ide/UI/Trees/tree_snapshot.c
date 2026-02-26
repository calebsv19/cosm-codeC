#include "ide/UI/Trees/tree_snapshot.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct SnapshotBuilder {
    char* data;
    size_t len;
    size_t cap;
} SnapshotBuilder;

static bool builder_reserve(SnapshotBuilder* b, size_t need_extra) {
    if (!b) return false;
    size_t needed = b->len + need_extra + 1;
    if (needed <= b->cap) return true;
    size_t new_cap = b->cap > 0 ? b->cap : 256;
    while (new_cap < needed) {
        new_cap *= 2;
    }
    char* next = (char*)realloc(b->data, new_cap);
    if (!next) return false;
    b->data = next;
    b->cap = new_cap;
    return true;
}

static bool builder_append(SnapshotBuilder* b, const char* text) {
    if (!b || !text) return false;
    size_t n = strlen(text);
    if (!builder_reserve(b, n)) return false;
    memcpy(b->data + b->len, text, n);
    b->len += n;
    b->data[b->len] = '\0';
    return true;
}

static bool builder_append_char(SnapshotBuilder* b, char ch) {
    if (!builder_reserve(b, 1)) return false;
    b->data[b->len++] = ch;
    b->data[b->len] = '\0';
    return true;
}

static bool append_node_line(SnapshotBuilder* b,
                             const UITreeNode* node,
                             bool include_indent) {
    if (!b || !node) return false;
    const char* label = node->label ? node->label : "";

    if (include_indent && node->depth > 0) {
        for (int i = 0; i < node->depth; ++i) {
            if (!builder_append(b, "  ")) return false;
        }
    }

    if (node->type == TREE_NODE_FOLDER || node->type == TREE_NODE_SECTION) {
        if (!builder_append(b, node->isExpanded ? "[-] " : "[+] ")) return false;
    }

    if (!builder_append(b, label)) return false;
    return builder_append_char(b, '\n');
}

static bool tree_append_visible(SnapshotBuilder* b,
                                const UITreeNode* node,
                                bool include_indent) {
    if (!b || !node) return true;
    if (!append_node_line(b, node, include_indent)) return false;

    if ((node->type == TREE_NODE_FOLDER || node->type == TREE_NODE_SECTION) && node->isExpanded) {
        for (int i = 0; i < node->childCount; ++i) {
            if (!tree_append_visible(b, node->children[i], include_indent)) return false;
        }
    }
    return true;
}

static size_t tree_count_visible_internal(const UITreeNode* node) {
    if (!node) return 0;
    size_t total = 1;
    if ((node->type == TREE_NODE_FOLDER || node->type == TREE_NODE_SECTION) && node->isExpanded) {
        for (int i = 0; i < node->childCount; ++i) {
            total += tree_count_visible_internal(node->children[i]);
        }
    }
    return total;
}

size_t tree_count_visible_nodes(const UITreeNode* root, bool include_root) {
    if (!root) return 0;
    if (include_root) return tree_count_visible_internal(root);
    size_t total = 0;
    if ((root->type == TREE_NODE_FOLDER || root->type == TREE_NODE_SECTION) && root->isExpanded) {
        for (int i = 0; i < root->childCount; ++i) {
            total += tree_count_visible_internal(root->children[i]);
        }
    }
    return total;
}

char* tree_build_visible_text_snapshot(const UITreeNode* root,
                                       const TreeSnapshotOptions* options) {
    if (!root) return NULL;
    TreeSnapshotOptions defaults = {
        .include_root = true,
        .include_indent = true
    };
    const TreeSnapshotOptions* opts = options ? options : &defaults;

    SnapshotBuilder b = {0};
    if (!builder_reserve(&b, 256)) return NULL;
    b.data[0] = '\0';

    bool ok = true;
    if (opts->include_root) {
        ok = tree_append_visible(&b, root, opts->include_indent);
    } else if ((root->type == TREE_NODE_FOLDER || root->type == TREE_NODE_SECTION) && root->isExpanded) {
        for (int i = 0; i < root->childCount; ++i) {
            if (!tree_append_visible(&b, root->children[i], opts->include_indent)) {
                ok = false;
                break;
            }
        }
    }

    if (!ok || b.len == 0) {
        free(b.data);
        return NULL;
    }
    return b.data;
}
