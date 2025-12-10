#include "ide/Panes/ToolPanels/Git/render_tool_git.h"
#include "ide/Panes/ToolPanels/Git/tool_git.h"
#include "ide/Panes/ToolPanels/Git/tree_git_adapter.h"

#include "engine/Render/render_helpers.h"
#include "engine/Render/render_pipeline.h"
#include "ide/UI/Trees/tree_renderer.h"
#include "ide/UI/scroll_manager.h"



#include <SDL2/SDL.h>
#include <string.h>

extern int mouseX;
extern int mouseY;

// === Static Tree Cache ===
UITreeNode* gitTree = NULL;
bool needsRefresh = true;
PaneScrollState gitScroll;
bool gitScrollInit = false;
SDL_Rect gitScrollTrack = {0};
SDL_Rect gitScrollThumb = {0};

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
        refreshGitLog(20);
        resetGitTree();      // Clears any existing tree
        needsRefresh = false;
    }

    if (!gitTree) {
        gitTree = convertGitModelToTree();
    }

    if (!gitScrollInit) {
        scroll_state_init(&gitScroll, NULL);
        gitScrollInit = true;
    }

    handleTreeMouseMove(mouseX, mouseY);
    renderTreePanelWithScroll(pane, gitTree, &gitScroll, &gitScrollTrack, &gitScrollThumb);
}
