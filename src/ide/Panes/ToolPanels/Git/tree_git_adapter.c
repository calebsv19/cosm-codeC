#include "tree_git_adapter.h"
#include "ide/Panes/ToolPanels/Git/tool_git.h"
#include <stdio.h>
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

    int fileCount = git_panel_file_count();
    for (int i = 0; i < fileCount; i++) {
        const GitFileEntry* f = git_panel_file_at(i);
        if (!f) continue;
        UITreeNode* node = createTreeNode(f->path, TREE_NODE_FILE, NODE_COLOR_DEFAULT, f->path, (void*)f);

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
    int logCount = git_panel_log_count();
    for (int i = 0; i < logCount; ++i) {
        const GitLogEntry* e = git_panel_log_at(i);
        if (!e) continue;
        char label[320];
        snprintf(label, sizeof(label), "%s %s", e->hash, e->message);
        UITreeNode* n = createTreeNode(label, TREE_NODE_SECTION, NODE_COLOR_DEFAULT, e->hash, (void*)e);
        n->isExpanded = false;

        char authorLine[196];
        snprintf(authorLine, sizeof(authorLine), "Author: %s", e->author[0] ? e->author : "unknown");
        UITreeNode* authorNode = createTreeNode(authorLine, TREE_NODE_FILE, NODE_COLOR_DEFAULT, NULL, (void*)e);
        addChildNode(n, authorNode);

        char dateLine[128];
        snprintf(dateLine, sizeof(dateLine), "Date: %s", e->date[0] ? e->date : "unknown");
        UITreeNode* dateNode = createTreeNode(dateLine, TREE_NODE_FILE, NODE_COLOR_DEFAULT, NULL, (void*)e);
        addChildNode(n, dateNode);

        addChildNode(logRoot, n);
    }

    if (git_panel_log_is_loading()) {
        char loadingLabel[128];
        snprintf(loadingLabel, sizeof(loadingLabel), "Loading full history... (%d loaded)", logCount);
        UITreeNode* loadingNode =
            createTreeNode(loadingLabel, TREE_NODE_FILE, NODE_COLOR_SECTION, NULL, NULL);
        addChildNode(logRoot, loadingNode);
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
