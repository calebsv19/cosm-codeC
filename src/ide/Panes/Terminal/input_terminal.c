#include "input_terminal.h"
#include "core/InputManager/input_macros.h" // adds CMD(InputCmd) abilities
#include "core/CommandBus/command_bus.h"
#include "ide/Panes/Terminal/command_terminal.h"


#include <stdio.h>


void handleTerminalKeyboardInput(UIPane* pane, SDL_Event* event) {
    if (!pane || event->type != SDL_KEYDOWN) return;

    SDL_Keycode key = event->key.keysym.sym;
    Uint16 mod = event->key.keysym.mod;

    if (mod & KMOD_CTRL) {
        switch (key) {
            case SDLK_l: CMD(COMMAND_CLEAR_TERMINAL); return;
            case SDLK_r: CMD(COMMAND_RUN_EXECUTABLE); return;
        }
    }

    printf("[Terminal] Unmapped keyboard input: %s\n", SDL_GetKeyName(key));
}


void handleTerminalMouseInput(UIPane* pane, SDL_Event* event) {
    if (event->type == SDL_MOUSEBUTTONDOWN) {
        printf("[Terminal] Mouse clicked at (%d, %d)\n", event->button.x, event->button.y);
    }
}


void handleTerminalScrollInput(UIPane* pane, SDL_Event* event) {
    (void)pane; (void)event;
}

void handleTerminalHoverInput(UIPane* pane, int x, int y) {
    (void)pane; (void)x; (void)y;
}

UIPaneInputHandler terminalInputHandler = {
    .onCommand = handleTerminalCommand,
    .onKeyboard = handleTerminalKeyboardInput,
    .onMouse = handleTerminalMouseInput,
    .onScroll = handleTerminalScrollInput,
    .onHover = handleTerminalHoverInput,
};


