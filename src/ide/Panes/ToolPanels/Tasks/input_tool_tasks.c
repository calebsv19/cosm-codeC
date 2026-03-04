#include "ide/Panes/ToolPanels/Tasks/input_tool_tasks.h"
#include "ide/Panes/ToolPanels/Tasks/tool_tasks.h"

#include "core/CommandBus/command_bus.h"
#include "ide/Panes/ToolPanels/Tasks/input_tool_tasks.h"
#include "core/InputManager/input_macros.h"
#include "ide/UI/input_modifiers.h"
#include "ide/UI/scroll_input_adapter.h"

void handleTasksKeyboardInput(UIPane* pane, SDL_Event* event) {
    if (!pane || event->type != SDL_KEYDOWN) return;

    SDL_Keycode key = event->key.keysym.sym;
    Uint16 mod = event->key.keysym.mod;

    if (ui_input_has_primary_accel(mod)) {
        switch (key) {
            case SDLK_n: CMD(COMMAND_ADD_TASK); return;
            case SDLK_r: CMD(COMMAND_RENAME_TASK); return;
            case SDLK_k: CMD(COMMAND_MOVE_TASK_UP); return;
            case SDLK_j: CMD(COMMAND_MOVE_TASK_DOWN); return;
            case SDLK_d: CMD(COMMAND_DELETE_TASK); return;
        }
    }

    switch (key) {
        case SDLK_DELETE: CMD(COMMAND_DELETE_TASK); return;
        case SDLK_RETURN: CMD(COMMAND_RENAME_TASK); return;
        default:
            printf("[TaskInput] Unmapped key: %d\n", key);
            break;
    }
}


void handleTasksMouseInput(UIPane* pane, SDL_Event* event) {
    if (!pane || !event) return;

    PaneScrollState* scroll = task_panel_scroll_state();
    SDL_Rect track = task_panel_scroll_track_rect();
    SDL_Rect thumb = task_panel_scroll_thumb_rect();
    if (ui_scroll_input_consume(scroll, event, &track, &thumb)) {
        return;
    }

    if (event->type != SDL_MOUSEBUTTONDOWN) return;

    if (event->button.button == SDL_BUTTON_LEFT) {
        handleTaskLeftClick(pane, event);
    } else if (event->button.button == SDL_BUTTON_RIGHT) {
	CMD(COMMAND_RENAME_TASK);

    }
}

void handleTasksScrollInput(UIPane* pane, SDL_Event* event) {
    if (!pane || !event) return;
    scroll_state_handle_mouse_wheel(task_panel_scroll_state(), event);
}



void handleTasksHoverInput(UIPane* pane, int x, int y) {
    handleTaskHoverAt(pane, x, y);
}

