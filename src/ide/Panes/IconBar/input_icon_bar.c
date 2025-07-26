#include "input_icon_bar.h"
#include "ide/Panes/IconBar/icon_bar.h"
#include "core/CommandBus/command_bus.h"
#include "core/InputManager/input_macros.h"


void handleIconBarKeyboardInput(UIPane* pane, SDL_Event* event) {
    SDL_Keycode key = event->key.keysym.sym;

    switch (key) {
        case SDLK_1: CMD(COMMAND_SELECT_ICON_PROJECT_FILES); break;
        case SDLK_2: CMD(COMMAND_SELECT_ICON_LIBRARIES); break;
        case SDLK_3: CMD(COMMAND_SELECT_ICON_BUILD_OUTPUT); break;
        case SDLK_4: CMD(COMMAND_SELECT_ICON_ERRORS); break;
        case SDLK_5: CMD(COMMAND_SELECT_ICON_ASSET_MANAGER); break;
        case SDLK_6: CMD(COMMAND_SELECT_ICON_TASKS); break;
        case SDLK_7: CMD(COMMAND_SELECT_ICON_VERSION_CONTROL); break;
        default: break;
    }
}



void handleIconBarMouseInput(UIPane* pane, SDL_Event* event) {
    if (event->type != SDL_MOUSEBUTTONDOWN) return;

    int mx = event->button.x;
    int my = event->button.y;

    for (int i = 0; i < ICON_COUNT; i++) {
        SDL_Rect rect = getIconRect(pane, i);
        if (mx >= rect.x && mx <= rect.x + rect.w &&
            my >= rect.y && my <= rect.y + rect.h) {
            switch ((IconTool)i) {
                case ICON_PROJECT_FILES:     CMD(COMMAND_SELECT_ICON_PROJECT_FILES); break;
                case ICON_LIBRARIES:         CMD(COMMAND_SELECT_ICON_LIBRARIES); break;
                case ICON_BUILD_OUTPUT:      CMD(COMMAND_SELECT_ICON_BUILD_OUTPUT); break;
                case ICON_ERRORS:            CMD(COMMAND_SELECT_ICON_ERRORS); break;
                case ICON_ASSET_MANAGER:     CMD(COMMAND_SELECT_ICON_ASSET_MANAGER); break;
                case ICON_TASKS:             CMD(COMMAND_SELECT_ICON_TASKS); break;
                case ICON_VERSION_CONTROL:   CMD(COMMAND_SELECT_ICON_VERSION_CONTROL); break;
                default: break;
            }
            break;
        }
    }
}


void handleIconBarScrollInput(UIPane* pane, SDL_Event* event) {
    // Reserved for future scrollable tool lists or nav
}

void handleIconBarHoverInput(UIPane* pane, int x, int y) {
    // Could be used to highlight hovered icon in future
}


// ==== Handler Struct Export ====
UIPaneInputHandler iconBarInputHandler = {
    .onCommand = NULL,
    .onKeyboard = handleIconBarKeyboardInput,
    .onMouse = handleIconBarMouseInput,
    .onScroll = handleIconBarScrollInput,
    .onHover = handleIconBarHoverInput
};

