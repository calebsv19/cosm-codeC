
#include "input_menu_bar.h"
#include "core/InputManager/input_macros.h" // adds CMD(InputCmd) abilities
#include "ide/Panes/MenuBar/menu_buttons.h"
#include "core/CommandBus/command_bus.h"
#include "engine/Render/render_text_helpers.h"

static inline bool isPointInRect(int x, int y, SDL_Rect rect) {
    return x >= rect.x && x <= rect.x + rect.w &&
           y >= rect.y && y <= rect.y + rect.h;
}


// ==== Keyboard ====
void handleMenuBarKeyboardInput(UIPane* pane, SDL_Event* event) {
    SDL_Keycode key = event->key.keysym.sym;
    Uint16 mod = event->key.keysym.mod;
    bool ctrl_or_cmd = (mod & (KMOD_CTRL | KMOD_GUI)) != 0;
    bool shift = (mod & KMOD_SHIFT) != 0;

    if (ctrl_or_cmd) {
        switch (key) {
            case SDLK_b:
                if (shift) {
                    CMD(COMMAND_BUILD_PROJECT);
                } else {
                    CMD(COMMAND_CHOOSE_WORKSPACE);
                }
                break;
            case SDLK_r: CMD(COMMAND_RUN_EXECUTABLE); break;
            case SDLK_d: CMD(COMMAND_DEBUG_EXECUTABLE); break;
            case SDLK_s: CMD(COMMAND_SAVE_FILE); break;
            case SDLK_l:
                if (shift) {
                    CMD(COMMAND_CHOOSE_WORKSPACE_TYPED);
                } else {
                    CMD(COMMAND_CHOOSE_WORKSPACE);
                }
                break;
        }
    }
}


// ==== Mouse ====
void handleMenuBarMouseInput(UIPane* pane, SDL_Event* event) {
    if (event->type != SDL_MOUSEBUTTONDOWN) return;

    int mx = event->button.x;
    int my = event->button.y;

    // === Right Buttons
    for (int i = 0; i < MENU_BUTTON_RIGHT_COUNT; i++) {
        SDL_Rect rect = getRightMenuButtonRect(pane, i);
        if (isPointInRect(mx, my, rect)) {

            switch ((MenuButtonRight)i) {
                case MENU_BUTTON_BUILD:          CMD(COMMAND_BUILD_PROJECT); break;
                case MENU_BUTTON_RUN:            CMD(COMMAND_RUN_EXECUTABLE); break;
                case MENU_BUTTON_DEBUG:          CMD(COMMAND_DEBUG_EXECUTABLE); break;
                case MENU_BUTTON_CONTROL_TOGGLE: CMD(COMMAND_TOGGLE_CONTROL_PANEL); break;
                default: break;
            }
            return;
        }
    }

    // === Left Buttons
    const char* fileName = getActiveFileName();
    int nameWidth = getTextWidth(fileName);
    int baseX = (currentMenuBarLayout == MENU_BAR_MODE_STANDARD)
        ? pane->x + 8 + nameWidth + 30
        : pane->x + 8;

    for (int i = 0; i < MENU_BUTTON_LEFT_COUNT; i++) {
        int x = baseX + i * (LEFT_BUTTON_WIDTH + BUTTON_SPACING);
        SDL_Rect rect = {
            x,
            pane->y + BUTTON_HEIGHT_PADDING / 2,
            LEFT_BUTTON_WIDTH,
            pane->h - BUTTON_HEIGHT_PADDING
        };

	if (isPointInRect(mx, my, rect)) {
            switch ((MenuButtonLeft)i) {
                case MENU_BUTTON_LOAD: CMD(COMMAND_CHOOSE_WORKSPACE); break;
                case MENU_BUTTON_SAVE: CMD(COMMAND_SAVE_FILE); break;
                default: break;
            }
            return;
        }
    }
}



// ==== Scroll ====
void handleMenuBarScrollInput(UIPane* pane, SDL_Event* event) {
    // Not used for now
}

// ==== Hover ====
void handleMenuBarHoverInput(UIPane* pane, int x, int y) {
    // Could be used to highlight button hover in future
}

// ==== Handler Struct ====
UIPaneInputHandler menuBarInputHandler = {
    .onCommand = NULL,  // Menu bar doesn't handle command dispatch directly
    .onKeyboard = handleMenuBarKeyboardInput,
    .onMouse = handleMenuBarMouseInput,
    .onScroll = handleMenuBarScrollInput,
    .onHover = handleMenuBarHoverInput
};
