#include "input_tool_git.h"
#include "ide/Panes/ToolPanels/Git/tool_git.h"
#include "ide/UI/Trees/tree_renderer.h"
#include "ide/UI/scroll_manager.h"
#include <stdio.h>
#include <string.h>

// Scroll state/rects are owned by render_tool_git.c
extern PaneScrollState gitScroll;
extern SDL_Rect gitScrollTrack;
extern SDL_Rect gitScrollThumb;
extern UITreeNode* gitTree;

void handleGitKeyboardInput(UIPane* pane, SDL_Event* event) {
    if (!pane || !event) return;

    if (event->type == SDL_TEXTINPUT && git_panel_is_message_focused()) {
        git_panel_insert_text(event->text.text);
        return;
    }

    if (event->type != SDL_KEYDOWN) return;
    SDL_Keycode key = event->key.keysym.sym;

    if (git_panel_is_message_focused()) {
        switch (key) {
            case SDLK_ESCAPE:
                git_panel_set_message_focus(false);
                return;
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                git_commit_with_message();
                return;
            case SDLK_BACKSPACE:
                git_panel_backspace();
                return;
            case SDLK_DELETE:
                git_panel_delete();
                return;
            case SDLK_LEFT:
                git_panel_move_cursor_left();
                return;
            case SDLK_RIGHT:
                git_panel_move_cursor_right();
                return;
            case SDLK_HOME:
                git_panel_move_cursor_home();
                return;
            case SDLK_END:
                git_panel_move_cursor_end();
                return;
            default:
                break;
        }
    }

    if (key == SDLK_ESCAPE) {
        git_panel_set_message_focus(false);
    }
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

        if (git_panel_point_in_add_all(mx, my)) {
            git_stage_all_changes();
            git_panel_set_message_focus(false);
            return;
        }
        if (git_panel_point_in_commit(mx, my)) {
            git_commit_with_message();
            git_panel_set_message_focus(false);
            return;
        }
        if (git_panel_point_in_message(mx, my)) {
            git_panel_set_message_focus(true);
            const char* msg = git_panel_get_message();
            git_panel_set_message(msg ? msg : "");
            return;
        }
        git_panel_set_message_focus(false);

        // Update hover state for downstream rendering
        handleTreeMouseMove(mx, my);

        // Hit-test with scroll awareness and select
        UIPane treePane = *pane;
        treePane.y = pane->y + GIT_PANEL_HEADER_HEIGHT - 30;
        treePane.h = (pane->y + pane->h) - treePane.y;
        if (treePane.h < 0) treePane.h = 0;
        handleTreeClickWithScroll(&treePane, gitTree, &gitScroll, mx, my);

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
	
