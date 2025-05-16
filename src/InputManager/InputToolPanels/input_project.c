// input_project.c
#include "input_project.h"
#include "ToolPanels/tool_project.h"
#include "CommandBus/command_bus.h"
#include "InputManager/input_macros.h"


void handleProjectFilesKeyboardInput(UIPane* pane, SDL_Event* event) {
    if (!pane || event->type != SDL_KEYDOWN) return;

    SDL_Keycode key = event->key.keysym.sym;
    Uint16 mod = event->key.keysym.mod;

    // === CTRL Shortcuts ===
    if (mod & KMOD_CTRL) {
        switch (key) {
            case SDLK_n: CMD(COMMAND_NEW_FILE); return;
            case SDLK_r: CMD(COMMAND_RENAME_FILE); return;
            case SDLK_o: CMD(COMMAND_OPEN_FILE); return;
        }
    }

    // === Return Key opens file ===
    if (key == SDLK_RETURN) {
        CMD(COMMAND_OPEN_FILE);
        return;
    }

    printf("[ProjectInput] Unmapped key: %s\n", SDL_GetKeyName(key));
}



void handleProjectFilesMouseInput(UIPane* pane, SDL_Event* event) {
    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
        int mx = event->button.x;
        handleProjectFilesClick(pane, mx);
    }
}


void handleProjectFilesScrollInput(UIPane* pane, SDL_Event* event) {
    // Optional scroll logic
}

void handleProjectFilesHoverInput(UIPane* pane, int x, int y) {
    updateHoveredMousePosition(x, y);
}

