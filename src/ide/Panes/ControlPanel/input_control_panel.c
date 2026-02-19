#include "input_control_panel.h"
#include "core/InputManager/input_macros.h" // adds CMD(InputCmd) abilities
#include "core/CommandBus/command_bus.h"
#include "ide/Panes/ControlPanel/control_panel.h"
#include "ide/Panes/ControlPanel/symbol_tree_adapter.h"
#include "ide/UI/Trees/tree_renderer.h"
#include "ide/Panes/Editor/Commands/editor_commands.h"
#include "app/GlobalInfo/core_state.h"
#include "fisics_frontend.h"

#include <stdio.h>

static Uint32 lastClickTime = 0;
static UITreeNode* lastClickNode = NULL;
static const Uint32 kDoubleClickMs = 350;

void handleControlPanelKeyboardInput(UIPane* pane, SDL_Event* event) {
    if (!pane || event->type != SDL_KEYDOWN) return;

    SDL_Keycode key = event->key.keysym.sym;
    Uint16 mod = event->key.keysym.mod;

    if (mod & KMOD_CTRL) {
        switch (key) {
            case SDLK_p: CMD(COMMAND_TOGGLE_LIVE_PARSE); return;
            case SDLK_e: CMD(COMMAND_TOGGLE_SHOW_ERRORS); return;
            case SDLK_m: CMD(COMMAND_TOGGLE_SHOW_MACROS); return;
        }
    }

    printf("[ControlPanel] Unmapped keyboard input: %d\n", key);
}


void handleControlPanelMouseInput(UIPane* pane, SDL_Event* event) {
    if (!pane || !event) return;

    PaneScrollState* scroll = control_panel_get_symbol_scroll();
    SDL_Rect* track = control_panel_get_symbol_scroll_track();
    SDL_Rect* thumb = control_panel_get_symbol_scroll_thumb();

    if (scroll_state_handle_mouse_drag(scroll, event, track, thumb)) {
        return;
    }

    if (event->type == SDL_MOUSEWHEEL) {
        scroll_state_handle_mouse_wheel(scroll, event);
        return;
    }

    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
        int mx = event->button.x;
        int my = event->button.y;
        handleTreeMouseMove(mx, my);

        UIPane listPane = *pane;
        int listTop = control_panel_get_symbol_list_top(pane);
        int listHeight = (pane->y + pane->h) - listTop;
        if (listHeight < 0) listHeight = 0;
        listPane.y = listTop;
        listPane.h = listHeight;

        UITreeNode* tree = control_panel_get_symbol_tree();
        if (tree) {
            handleTreeClickWithScroll(&listPane, tree, scroll, mx, my);
            UITreeNode* selected = getSelectedTreeNode();
            if (selected) {
                symbol_tree_cache_note_node(selected);
                Uint32 now = SDL_GetTicks();
                bool isDoubleClick = (selected == lastClickNode) &&
                                     (now - lastClickTime <= kDoubleClickMs);
                lastClickTime = now;
                lastClickNode = selected;

                if (isDoubleClick) {
                    EditorView* view = getCoreState()->activeEditorView;
                    const FisicsSymbol* sym = (const FisicsSymbol*)selected->userData;
                    if (view && sym && sym->file_path) {
                        editor_jump_to(view, sym->file_path, sym->start_line, sym->start_col);
                    } else if (view && selected->fullPath &&
                               selected->type != TREE_NODE_FOLDER) {
                        editor_jump_to(view, selected->fullPath, 0, 0);
                    }
                }
            }
        }
    }
}


void handleControlPanelScrollInput(UIPane* pane, SDL_Event* event) {
    if (!pane || !event) return;
    PaneScrollState* scroll = control_panel_get_symbol_scroll();
    scroll_state_handle_mouse_wheel(scroll, event);
}

void handleControlPanelHoverInput(UIPane* pane, int x, int y) {
    (void)pane;
    handleTreeMouseMove(x, y);
}

UIPaneInputHandler controlPanelInputHandler = {
    .onCommand = NULL,
    .onKeyboard = handleControlPanelKeyboardInput,
    .onMouse = handleControlPanelMouseInput,
    .onScroll = handleControlPanelScrollInput,
    .onHover = handleControlPanelHoverInput,
};
