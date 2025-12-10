#include "input_tool_git.h"
#include "ide/Panes/ToolPanels/Git/tool_git.h"
#include "ide/UI/Trees/tree_renderer.h"
#include "ide/UI/scroll_manager.h"
#include <stdio.h>

// Scroll state/rects are owned by render_tool_git.c
extern PaneScrollState gitScroll;
extern SDL_Rect gitScrollTrack;
extern SDL_Rect gitScrollThumb;
extern UITreeNode* gitTree;

void handleGitKeyboardInput(UIPane* pane, SDL_Event* event) {
    (void)pane; (void)event;
}

void handleGitMouseInput(UIPane* pane, SDL_Event* event) {
    if (!pane || !event) return;

    // Thumb drag support
    if (scroll_state_handle_mouse_drag(&gitScroll, event, &gitScrollTrack, &gitScrollThumb)) {
        return;
    }

    if (event->type == SDL_MOUSEWHEEL) {
        scroll_state_handle_mouse_wheel(&gitScroll, event);
        return;
    }

    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
        int mx = event->button.x;
        int my = event->button.y;

        // Update hover state for downstream rendering
        handleTreeMouseMove(mx, my);

        // Hit-test with scroll awareness and select
        handleTreeClickWithScroll(pane, gitTree, &gitScroll, mx, my);

        UITreeNode* selected = getSelectedTreeNode();

        if (selected && selected->type == TREE_NODE_FILE) {
            printf("[GitPanel] Clicked: %s\n", selected->label);
            // TODO: Optional: trigger stage/unstage here
        }
    }
}

void handleGitScrollInput(UIPane* pane, SDL_Event* event) {
    (void)pane;
    if (!event) return;
    scroll_state_handle_mouse_wheel(&gitScroll, event);
}

void handleGitHoverInput(UIPane* pane, int x, int y) {
    handleTreeMouseMove(x, y); // l ive hover tracking
}
	
