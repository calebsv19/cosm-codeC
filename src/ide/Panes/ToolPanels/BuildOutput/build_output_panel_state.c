#include "build_output_panel_state.h"
#include <stdlib.h>

static BuildOutputPanelState state = {0};

void initBuildOutputPanelState(void) {
    state.rootTree = NULL;
    state.selectedBuildDirectory = NULL;
    state.selectedRunTarget = NULL;
    state.selectedDiagIndex = -1;
}

void freeBuildOutputPanelState(void) {
    if (state.rootTree) {
        freeTreeNodeRecursive(state.rootTree);
        state.rootTree = NULL;
    }
    state.selectedBuildDirectory = NULL;
    state.selectedRunTarget = NULL;
    state.selectedDiagIndex = -1;
}

BuildOutputPanelState* getBuildOutputPanelState(void) {
    return &state;
}

void setBuildOutputRoot(UITreeNode* root) {
    if (state.rootTree) {
        freeTreeNodeRecursive(state.rootTree);
    }
    state.rootTree = root;
}

void selectBuildDirectory(UITreeNode* dirNode) {
    if (dirNode && dirNode->type == TREE_NODE_FOLDER) {
        state.selectedBuildDirectory = dirNode;
    }
}

void selectRunTarget(UITreeNode* fileNode) {
    if (fileNode && fileNode->type == TREE_NODE_FILE) {
        state.selectedRunTarget = fileNode;
    }
}

UITreeNode* getSelectedBuildDirectory(void) {
    return state.selectedBuildDirectory;
}

UITreeNode* getSelectedRunTarget(void) {
    return state.selectedRunTarget;
}

int getSelectedBuildDiag(void) {
    return state.selectedDiagIndex;
}

void setSelectedBuildDiag(int index) {
    state.selectedDiagIndex = index;
}
