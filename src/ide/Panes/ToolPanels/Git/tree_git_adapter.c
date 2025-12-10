#include "tree_git_adapter.h"
#include "ide/Panes/ToolPanels/Git/tool_git.h"
#include <string.h>
#include <stdlib.h>

static UITreeNode* createSection(const char* label, TreeNodeColor color, bool expanded) {
    UITreeNode* node = createTreeNode(label, TREE_NODE_SECTION, color, NULL, NULL);
    node->isExpanded = expanded;
    return node;
}

UITreeNode* convertGitModelToTree(void) {
    UITreeNode* root = createTreeNode("Git", TREE_NODE_SECTION, NODE_COLOR_SECTION, NULL, NULL);

    // Changes section
    UITreeNode* changesRoot = createSection("Changes", NODE_COLOR_SECTION, true);
    UITreeNode* staged = createSection("Staged", NODE_COLOR_STAGED, true);
    UITreeNode* modified = createSection("Modified", NODE_COLOR_MODIFIED, true);
    UITreeNode* added = createSection("Added", NODE_COLOR_ADDED, true);
    UITreeNode* deleted = createSection("Deleted", NODE_COLOR_DELETED, true);
    UITreeNode* untracked = createSection("Untracked", NODE_COLOR_UNTRACKED, true);

    for (int i = 0; i < gitFileCount; i++) {
        GitFileEntry* f = &gitFiles[i];
        UITreeNode* node = createTreeNode(f->path, TREE_NODE_FILE, NODE_COLOR_DEFAULT, f->path, f);

        switch (f->status) {
            case GIT_STATUS_STAGED:    node->color = NODE_COLOR_STAGED;    addChildNode(staged, node); break;
            case GIT_STATUS_MODIFIED:  node->color = NODE_COLOR_MODIFIED;  addChildNode(modified, node); break;
            case GIT_STATUS_ADDED:     node->color = NODE_COLOR_ADDED;     addChildNode(added, node); break;
            case GIT_STATUS_DELETED:   node->color = NODE_COLOR_DELETED;   addChildNode(deleted, node); break;
            case GIT_STATUS_UNTRACKED: node->color = NODE_COLOR_UNTRACKED; addChildNode(untracked, node); break;
            default: freeTreeNodeRecursive(node); break;
        }
    }

    if (staged->childCount)    addChildNode(changesRoot, staged);    else freeTreeNodeRecursive(staged);
    if (modified->childCount)  addChildNode(changesRoot, modified);  else freeTreeNodeRecursive(modified);
    if (added->childCount)     addChildNode(changesRoot, added);     else freeTreeNodeRecursive(added);
    if (deleted->childCount)   addChildNode(changesRoot, deleted);   else freeTreeNodeRecursive(deleted);
    if (untracked->childCount) addChildNode(changesRoot, untracked); else freeTreeNodeRecursive(untracked);

    if (changesRoot->childCount) {
        addChildNode(root, changesRoot);
    } else {
        freeTreeNodeRecursive(changesRoot);
    }

    // Log section
    UITreeNode* logRoot = createSection("Log", NODE_COLOR_SECTION, false);
    for (int i = 0; i < gitLogCount; ++i) {
        GitLogEntry* e = &gitLogEntries[i];
        char label[320];
        snprintf(label, sizeof(label), "%s %s", e->hash, e->message);
        UITreeNode* n = createTreeNode(label, TREE_NODE_FILE, NODE_COLOR_DEFAULT, e->hash, e);
        addChildNode(logRoot, n);
    }
    if (logRoot->childCount) {
        addChildNode(root, logRoot);
    } else {
        freeTreeNodeRecursive(logRoot);
    }

    return root;
}

void freeGitTree(UITreeNode* root) {
    freeTreeNodeRecursive(root);
}
