#include "input_tool_git.h"
#include "ide/Panes/ToolPanels/Git/tool_git.h"
#include "ide/UI/Trees/tree_renderer.h"    
#include <stdio.h>

void handleGitKeyboardInput(UIPane* pane, SDL_Event* event) {
    (void)pane; (void)event;
}

void handleGitMouseInput(UIPane* pane, SDL_Event* event) {
    if (!pane || event->type != SDL_MOUSEBUTTONDOWN) return;

    if (event->button.button == SDL_BUTTON_LEFT) {
        int mx = event->button.x;
        int my = event->button.y;

        handleTreeClick(pane, mx, my);  //  select hovered
        UITreeNode* selected = getSelectedTreeNode();

        if (selected && selected->type == TREE_NODE_FILE) {
            printf("[GitPanel] Clicked: %s\n", selected->label);
            // TODO: Optional: trigger stage/unstage here
        }
    }
}

void handleGitScrollInput(UIPane* pane, SDL_Event* event) {
    (void)pane; (void)event;
}

void handleGitHoverInput(UIPane* pane, int x, int y) {
    handleTreeMouseMove(x, y); // l ive hover tracking
}
	
