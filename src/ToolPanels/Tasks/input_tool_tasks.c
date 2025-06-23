#include "ToolPanels/Tasks/input_tool_tasks.h"
#include "ToolPanels/Tasks/tool_tasks.h"

#include "CommandBus/command_bus.h"
#include "ToolPanels/Tasks/input_tool_tasks.h"
#include "InputManager/input_macros.h"

void handleTasksKeyboardInput(UIPane* pane, SDL_Event* event) {
    if (!pane || event->type != SDL_KEYDOWN) return;

    SDL_Keycode key = event->key.keysym.sym;
    Uint16 mod = event->key.keysym.mod;

    if (mod & KMOD_CTRL) {
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
    if (!event) return;

    switch (event->type) {
        case SDL_MOUSEBUTTONDOWN:
            handleTaskMouseClick(pane, event);
            break;
        default:
            break;
    }
}

void handleTasksScrollInput(UIPane* pane, SDL_Event* event) {
    // Tasks panel currently has no scroll behavior
    (void)pane;
    (void)event;
}



void handleTasksHoverInput(UIPane* pane, int x, int y) {
    // mimic MOUSEMOTION event to reuse existing logic
    SDL_Event fakeMotion = {0};
    fakeMotion.type = SDL_MOUSEMOTION;
    fakeMotion.motion.x = x;
    fakeMotion.motion.y = y;

    handleTaskMouseMotion(pane, &fakeMotion);
}



