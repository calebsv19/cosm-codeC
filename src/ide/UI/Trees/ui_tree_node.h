#ifndef UI_TREE_NODE_H
#define UI_TREE_NODE_H

#include <stdbool.h>

typedef void (*UITreeUserDataFreeFn)(void*);

// Types of nodes (file, folder, group/section, etc.)
typedef enum {
    TREE_NODE_UNKNOWN = 0,
    TREE_NODE_FILE,
    TREE_NODE_FOLDER,
    TREE_NODE_SECTION  // Used for headers like "Staged", "Modified"
} TreeNodeType;

// Optional color enum for semantic meaning (e.g., git status)
typedef enum {
    NODE_COLOR_DEFAULT = 0,
    NODE_COLOR_MODIFIED,
    NODE_COLOR_ADDED,
    NODE_COLOR_DELETED,
    NODE_COLOR_UNTRACKED,
    NODE_COLOR_STAGED,
    NODE_COLOR_SECTION
} TreeNodeColor;

// Core tree node structure
typedef struct UITreeNode {
    TreeNodeType type;
    TreeNodeColor color;    // Optional semantic coloring
    const char* label;      // e.g., "main.c" or "Staged"
    const char* fullPath;   // e.g., "src/main.c" or NULL for virtual nodes
    bool isExpanded;        // Folders or sections
    int depth;              // Visual indent level
    void* userData;         // Back-reference to original data (DirEntry, GitFileEntry)
    UITreeUserDataFreeFn userDataFreeFn;

    struct UITreeNode** children;
    int childCount;
    int childCapacity;
} UITreeNode;

// === Utility Creation API ===
UITreeNode* createTreeNode(const char* label, TreeNodeType type, TreeNodeColor color, const char* fullPath, void* 
userData);
void setTreeNodeUserDataFreeFn(UITreeNode* node, UITreeUserDataFreeFn fn);
void addChildNode(UITreeNode* parent, UITreeNode* child);
void freeTreeNodeRecursive(UITreeNode* node);

#endif // UI_TREE_NODE_H
