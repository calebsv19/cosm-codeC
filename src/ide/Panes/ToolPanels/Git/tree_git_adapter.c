#include "tree_git_adapter.h"
#include "ide/Panes/ToolPanels/Git/tool_git.h"
#include <string.h>
#include <stdlib.h>

static UITreeNode* createStatusSection(const char* label, TreeNodeColor color) {
    UITreeNode* node = createTreeNode(label, TREE_NODE_SECTION, color, NULL, NULL);
    node->isExpanded = true;
    return node;
}

UITreeNode* convertGitStatusToTree(void) {
    UITreeNode* root = createTreeNode("Git Changes", TREE_NODE_SECTION, NODE_COLOR_SECTION, NULL, NULL);

    UITreeNode* staged = createStatusSection("Staged", NODE_COLOR_STAGED);
    UITreeNode* modified = createStatusSection("Modified", NODE_COLOR_MODIFIED);
    UITreeNode* added = createStatusSection("Added", NODE_COLOR_ADDED);
    UITreeNode* deleted = createStatusSection("Deleted", NODE_COLOR_DELETED);
    UITreeNode* untracked = createStatusSection("Untracked", NODE_COLOR_UNTRACKED);

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

    if (staged->childCount)    addChildNode(root, staged);    else freeTreeNodeRecursive(staged);
    if (modified->childCount)  addChildNode(root, modified);  else freeTreeNodeRecursive(modified);
    if (added->childCount)     addChildNode(root, added);     else freeTreeNodeRecursive(added);
    if (deleted->childCount)   addChildNode(root, deleted);   else freeTreeNodeRecursive(deleted);
    if (untracked->childCount) addChildNode(root, untracked); else freeTreeNodeRecursive(untracked);

    printf("[GitTree] root has %d children\n", root->childCount);
	for (int i = 0; i < root->childCount; i++) {
	    printf("  [%d] section: %s\n", i, root->children[i]->label);
	}

    return root;
}

void freeGitTree(UITreeNode* root) {
    freeTreeNodeRecursive(root);
}

