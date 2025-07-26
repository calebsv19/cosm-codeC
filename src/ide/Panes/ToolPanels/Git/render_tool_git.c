#include "ide/Panes/ToolPanels/Git/render_tool_git.h"
#include "ide/Panes/ToolPanels/Git/tool_git.h"
#include "ide/Panes/ToolPanels/Git/tree_git_adapter.h"

#include "engine/Render/render_helpers.h"
#include "engine/Render/render_pipeline.h"
#include "ide/UI/Trees/tree_renderer.h"



#include <SDL2/SDL.h>
#include <string.h>

extern int mouseX;
extern int mouseY;

// === Static Tree Cache ===
static UITreeNode* gitTree = NULL;
static bool needsRefresh = true;

void resetGitTree(void) {
    if (gitTree) {
        freeGitTree(gitTree);
        gitTree = NULL;
    }
    needsRefresh = true; // mark for refresh
}

void renderGitPanel(UIPane* pane) {
    if (needsRefresh) {
        refreshGitStatus();  // Now runs before the tree is built
        resetGitTree();      // Clears any existing tree
        needsRefresh = false;
    }

    if (!gitTree) {
        gitTree = convertGitStatusToTree();
    }

    handleTreeMouseMove(mouseX, mouseY);
    renderTreePanel(pane, gitTree);
}
