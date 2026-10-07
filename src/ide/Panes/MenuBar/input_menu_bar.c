
#include "input_menu_bar.h"
#include "core/InputManager/input_macros.h" // adds CMD(InputCmd) abilities
#include "ide/Panes/MenuBar/menu_buttons.h"
#include "core/CommandBus/command_bus.h"
#include "engine/Render/render_text_helpers.h"

#include "menu_bar_controls.h"
#include "kit_ui_interaction_sdl.h"
#include "app/GlobalInfo/core_state.h"
#include "core/InputManager/UserInput/rename_flow.h"
#include <string.h>

static KitUiInteractionContext interaction;
static KitUiInteractionControl controls[IDE_MENU_CONTROL_COUNT];
static UIPane* owner;
static bool geometry_valid;

void ide_menu_bar_cancel(void) {
    kit_ui_interaction_reset(&interaction);
    geometry_valid = false;
}

static bool menu_scope_available(UIPane* pane) {
    IDECoreState* core = getCoreState();
    return pane && pane->visible && !isRenaming() &&
           (!core || !core->workspaceAuthoring.active);
}

static void sync_controls(UIPane* pane) {
    KitUiInteractionControl next[IDE_MENU_CONTROL_COUNT];
    ide_menu_bar_geometry(pane, currentMenuBarLayout, getTextWidth(getActiveFileName()), next);
    if (!geometry_valid || owner != pane || memcmp(controls, next, sizeof(next)) != 0) {
        kit_ui_interaction_reset(&interaction);
        memcpy(controls, next, sizeof(next));
        owner = pane;
        geometry_valid = true;
    }
}

static bool route_menu_event(UIPane* pane, SDL_Event* event) {
    if (!menu_scope_available(pane)) { ide_menu_bar_cancel(); return false; }
    sync_controls(pane);
    KitUiInteractionEvent input;
    KitUiInteractionResult result = {0};
    if (!kit_ui_interaction_event_from_sdl(event, &input)) return false;
    CoreResult status = kit_ui_interaction_route(&interaction, controls, IDE_MENU_CONTROL_COUNT,
                                                &input, &result);
    if (status.code != CORE_OK) { ide_menu_bar_cancel(); return false; }
    if (result.activated_id) CMD(ide_menu_bar_command(result.activated_id));
    return result.consumed != 0;
}

KitUiButtonState ide_menu_bar_button_state(UIPane* pane, int index) {
    if (!menu_scope_available(pane) || index < 0 || index >= IDE_MENU_CONTROL_COUNT) {
        ide_menu_bar_cancel();
        return (KitUiButtonState){0};
    }
    sync_controls(pane);
    KitUiButtonState state = kit_ui_interaction_button_state(&interaction, &controls[index], 0);
    IDECoreState* core = getCoreState();
    if (core && core->focusedPane != pane) state.focused = 0;
    return state;
}

// ==== Keyboard ====
void handleMenuBarKeyboardInput(UIPane* pane, SDL_Event* event) {
    if (!menu_scope_available(pane)) { ide_menu_bar_cancel(); return; }
    if (route_menu_event(pane, event)) return;
    if (event->type != SDL_KEYDOWN) return;
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
    (void)route_menu_event(pane, event);
}

// ==== Scroll ====
void handleMenuBarScrollInput(UIPane* pane, SDL_Event* event) {
    // Not used for now
}

// ==== Hover ====
void handleMenuBarHoverInput(UIPane* pane, int x, int y) {
    SDL_Event event = {0};
    event.type = SDL_MOUSEMOTION;
    event.motion.x = x;
    event.motion.y = y;
    (void)route_menu_event(pane, &event);
}

// ==== Handler Struct ====
UIPaneInputHandler menuBarInputHandler = {
    .onCommand = NULL,  // Menu bar doesn't handle command dispatch directly
    .onKeyboard = handleMenuBarKeyboardInput,
    .onMouse = handleMenuBarMouseInput,
    .onScroll = handleMenuBarScrollInput,
    .onHover = handleMenuBarHoverInput
};
