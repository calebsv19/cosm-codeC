#ifndef BUILD_OUTPUT_PANEL_STATE_H
#define BUILD_OUTPUT_PANEL_STATE_H

#include "ide/UI/Trees/ui_tree_node.h"

// Central panel state
typedef struct {
    UITreeNode* rootTree;
    UITreeNode* selectedBuildDirectory;
    UITreeNode* selectedRunTarget;
} BuildOutputPanelState;

// === Lifecycle
void initBuildOutputPanelState(void);
void freeBuildOutputPanelState(void);

// === Accessors
BuildOutputPanelState* getBuildOutputPanelState(void);

// === Setters
void setBuildOutputRoot(UITreeNode* root);
void selectBuildDirectory(UITreeNode* dirNode);
void selectRunTarget(UITreeNode* fileNode);

// === Getters
UITreeNode* getSelectedBuildDirectory(void);
UITreeNode* getSelectedRunTarget(void);

#endif // BUILD_OUTPUT_PANEL_STATE_H

