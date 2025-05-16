#include "input_control_panel.h"
#include "InputManager/input_macros.h" // adds CMD(InputCmd) abilities
#include "CommandBus/command_bus.h"

#include <stdio.h>

void handleControlPanelKeyboardInput(UIPane* pane, SDL_Event* event) {
    if (!pane || event->type != SDL_KEYDOWN) return;

    SDL_Keycode key = event->key.keysym.sym;
    Uint16 mod = event->key.keysym.mod;

    if (mod & KMOD_CTRL) {
        switch (key) {
            case SDLK_p: CMD(COMMAND_TOGGLE_LIVE_PARSE); return;
            case SDLK_e: CMD(COMMAND_TOGGLE_SHOW_ERRORS); return;
        }
    }

    printf("[ControlPanel] Unmapped keyboard input: %d\n", key);
}


// Example structure for future expansion
void handleControlPanelMouseInput(UIPane* pane, SDL_Event* event) {
    if (!pane || event->type != SDL_MOUSEBUTTONDOWN) return;

    int mx = event->button.x;
    int my = event->button.y;

    // Later: if (isInsideLiveParseButton(mx, my)) CMD(COMMAND_TOGGLE_LIVE_PARSE);
}


void handleControlPanelScrollInput(UIPane* pane, SDL_Event* event) {
    (void)pane; (void)event;
}

void handleControlPanelHoverInput(UIPane* pane, int x, int y) {
    (void)pane; (void)x; (void)y;
}

UIPaneInputHandler controlPanelInputHandler = {
    .onCommand = NULL,
    .onKeyboard = handleControlPanelKeyboardInput,
    .onMouse = handleControlPanelMouseInput,
    .onScroll = handleControlPanelScrollInput,
    .onHover = handleControlPanelHoverInput,
};

