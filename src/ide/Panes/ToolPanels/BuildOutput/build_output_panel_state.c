#include "build_output_panel_state.h"
#include "ide/Panes/ToolPanels/tool_panel_adapter.h"
#include <stdbool.h>
#include <stdlib.h>

static BuildOutputPanelState g_buildOutputPanelBootstrapState = {0};
static bool g_buildOutputPanelBootstrapInitialized = false;

static void build_output_panel_state_init(void* ptr) {
    BuildOutputPanelState* state = (BuildOutputPanelState*)ptr;
    if (!state) return;
    state->rootTree = NULL;
    state->selectedBuildDirectory = NULL;
    state->selectedRunTarget = NULL;
    state->selectedDiagIndex = -1;
}

static void build_output_panel_state_destroy(void* ptr) {
    BuildOutputPanelState* state = (BuildOutputPanelState*)ptr;
    if (!state) return;
    if (state->rootTree) {
        freeTreeNodeRecursive(state->rootTree);
        state->rootTree = NULL;
    }
    free(state);
}

static BuildOutputPanelState* build_output_panel_state(void) {
    return (BuildOutputPanelState*)tool_panel_resolve_state_slot(
        TOOL_PANEL_STATE_SLOT_BUILD_OUTPUT_PANEL,
        sizeof(BuildOutputPanelState),
        build_output_panel_state_init,
        build_output_panel_state_destroy,
        &g_buildOutputPanelBootstrapState,
        &g_buildOutputPanelBootstrapInitialized
    );
}

void initBuildOutputPanelState(void) {
    build_output_panel_state_init(build_output_panel_state());
}

void freeBuildOutputPanelState(void) {
    BuildOutputPanelState* state = build_output_panel_state();
    if (state->rootTree) {
        freeTreeNodeRecursive(state->rootTree);
        state->rootTree = NULL;
    }
    state->selectedBuildDirectory = NULL;
    state->selectedRunTarget = NULL;
    state->selectedDiagIndex = -1;
}

BuildOutputPanelState* getBuildOutputPanelState(void) {
    return build_output_panel_state();
}

void setBuildOutputRoot(UITreeNode* root) {
    BuildOutputPanelState* state = build_output_panel_state();
    if (state->rootTree) {
        freeTreeNodeRecursive(state->rootTree);
    }
    state->rootTree = root;
}

void selectBuildDirectory(UITreeNode* dirNode) {
    BuildOutputPanelState* state = build_output_panel_state();
    if (dirNode && dirNode->type == TREE_NODE_FOLDER) {
        state->selectedBuildDirectory = dirNode;
    }
}

void selectRunTarget(UITreeNode* fileNode) {
    BuildOutputPanelState* state = build_output_panel_state();
    if (fileNode && fileNode->type == TREE_NODE_FILE) {
        state->selectedRunTarget = fileNode;
    }
}

UITreeNode* getSelectedBuildDirectory(void) {
    return build_output_panel_state()->selectedBuildDirectory;
}

UITreeNode* getSelectedRunTarget(void) {
    return build_output_panel_state()->selectedRunTarget;
}

int getSelectedBuildDiag(void) {
    return build_output_panel_state()->selectedDiagIndex;
}

void setSelectedBuildDiag(int index) {
    build_output_panel_state()->selectedDiagIndex = index;
}
