#include "input_tool_git.h"
#include "ide/Panes/ToolPanels/Git/tool_git.h"
#include "ide/UI/editor_navigation.h"
#include "ide/UI/input_modifiers.h"
#include "ide/UI/panel_control_widgets.h"
#include "ide/UI/row_activation.h"
#include "ide/UI/scroll_input_adapter.h"
#include "ide/UI/Trees/tree_renderer.h"
#include "ide/UI/Trees/tree_snapshot.h"
#include "ide/UI/scroll_manager.h"
#include "core/Clipboard/clipboard.h"
#include "app/GlobalInfo/core_state.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef struct {
    UITreeNode* root;
    UITreeNode* node;
} GitTreeActivationState;

static void git_tree_select_single(void* user_data) {
    GitTreeActivationState* state = (GitTreeActivationState*)user_data;
    if (!state || !state->node) return;
    if (tree_select_all_visual_active_for(state->root)) {
        clearTreeSelectAllVisual();
    }
    setSelectedTreeNode(state->node);
}

static void git_tree_prefix_action(void* user_data) {
    GitTreeActivationState* state = (GitTreeActivationState*)user_data;
    if (!state || !state->node) return;
    if (state->node->type == TREE_NODE_FOLDER || state->node->type == TREE_NODE_SECTION) {
        state->node->isExpanded = !state->node->isExpanded;
    }
}

static void git_tree_activate(void* user_data) {
    GitTreeActivationState* state = (GitTreeActivationState*)user_data;
    if (!state || !state->node) return;
    if (state->node->type == TREE_NODE_FILE && state->node->fullPath && state->node->fullPath[0]) {
        (void)ui_open_path_at_location_in_best_editor_view(state->node->fullPath, 0, 0);
    }
}

void handleGitKeyboardInput(UIPane* pane, SDL_Event* event) {
    if (!pane || !event) return;

    if (event->type == SDL_TEXTINPUT && git_panel_is_message_focused()) {
        (void)git_panel_handle_message_text_input(event);
        return;
    }

    if (event->type != SDL_KEYDOWN) return;
    SDL_Keycode key = event->key.keysym.sym;
    Uint16 mod = event->key.keysym.mod;
    bool accel = ui_input_has_primary_accel(mod);

    if (git_panel_is_message_focused()) {
        if (git_panel_handle_message_edit_key(key)) {
            return;
        }
        switch (key) {
            case SDLK_ESCAPE:
                git_panel_set_message_focus(false);
                return;
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                git_commit_with_message();
                return;
            default:
                break;
        }
    }

    IDECoreState* core = getCoreState();
    int mx = core ? core->mouseX : -1;
    int my = core ? core->mouseY : -1;
    int treeTop = git_panel_tree_content_top(pane);
    bool inTreeArea = (mx >= pane->x && mx <= (pane->x + pane->w) &&
                       my >= treeTop && my <= (pane->y + pane->h));
    if (accel && inTreeArea) {
        if (key == SDLK_a) {
            setTreeSelectAllVisualRoot(git_panel_tree());
            return;
        }
        if (key == SDLK_c && git_panel_tree()) {
            TreeSnapshotOptions opts = { .include_root = true, .include_indent = true };
            char* snapshot = tree_build_visible_text_snapshot(git_panel_tree(), &opts);
            if (snapshot && snapshot[0]) {
                clipboard_copy_text(snapshot);
            }
            free(snapshot);
            return;
        }
    }

    if (key == SDLK_ESCAPE) {
        git_panel_set_message_focus(false);
    }
}

void handleGitMouseInput(UIPane* pane, SDL_Event* event) {
    if (!pane || !event) return;

    if (ui_scroll_input_consume(git_panel_scroll(),
                                event,
                                git_panel_scroll_track(),
                                git_panel_scroll_thumb())) {
        return;
    }

    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
        int mx = event->button.x;
        int my = event->button.y;
        UIPanelThreeSegmentStripLayout topStrip = git_panel_get_top_strip_layout();

        if (ui_panel_rect_contains(&topStrip.leading_rect, mx, my)) {
            git_stage_all_changes();
            git_panel_set_message_focus(false);
            return;
        }
        if (ui_panel_rect_contains(&topStrip.trailing_rect, mx, my)) {
            git_commit_with_message();
            git_panel_set_message_focus(false);
            return;
        }
        if (ui_panel_rect_contains(&topStrip.middle_rect, mx, my)) {
            git_panel_set_message_focus(true);
            const char* msg = git_panel_get_message();
            git_panel_set_message(msg ? msg : "");
            return;
        }
        git_panel_set_message_focus(false);

        // Update hover state for downstream rendering
        handleTreeMouseMove(mx, my);

        UIPane treePane = {0};
        git_panel_tree_viewport(pane, &treePane);
        UITreeNode* root = git_panel_tree();
        UITreeNode* hit = hitTestTreeNodeWithScroll(&treePane, root, git_panel_scroll(), mx, my);
        if (!hit) {
            return;
        }

        GitTreeActivationState activation = {
            .root = root,
            .node = hit
        };
        (void)ui_row_activation_handle_primary(
            &(UIRowActivationContext){
                .double_click_tracker = git_panel_tree_double_click_tracker(),
                .row_identity = (uintptr_t)hit,
                .double_click_ms = UI_DOUBLE_CLICK_MS_DEFAULT,
                .clicked_prefix = treeNodePrefixHit(&treePane, hit, mx),
                .additive_modifier = false,
                .range_modifier = false,
                .wants_drag_start = false,
                .on_select_single = git_tree_select_single,
                .on_prefix = git_tree_prefix_action,
                .on_activate = git_tree_activate,
                .user_data = &activation
            });
    }
}

void handleGitScrollInput(UIPane* pane, SDL_Event* event) {
    (void)pane;
    if (!event) return;
    scroll_state_handle_mouse_wheel(git_panel_scroll(), event);
}

void handleGitHoverInput(UIPane* pane, int x, int y) {
    handleTreeMouseMove(x, y); // l ive hover tracking
}
	
