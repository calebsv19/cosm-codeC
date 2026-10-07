#include "input_icon_bar.h"
#include "icon_bar.h"
#include "ide/UI/button_scope.h"
#include "core/InputManager/input_macros.h"
#include "app/GlobalInfo/core_state.h"
#include "core/InputManager/UserInput/rename_flow.h"
static IDEButtonScope scope;
static const InputCommand commands[ICON_COUNT] = {
    COMMAND_SELECT_ICON_PROJECT_FILES, COMMAND_SELECT_ICON_LIBRARIES,
    COMMAND_SELECT_ICON_BUILD_OUTPUT, COMMAND_SELECT_ICON_ERRORS,
    COMMAND_SELECT_ICON_ASSET_MANAGER, COMMAND_SELECT_ICON_TASKS,
    COMMAND_SELECT_ICON_VERSION_CONTROL
};
static void prepare(UIPane* pane) {
    KitUiInteractionControl controls[ICON_COUNT] = {0};
    for (int i = 0; i < ICON_COUNT; ++i) {
        SDL_Rect r = getIconRect(pane, i);
        controls[i] = (KitUiInteractionControl){.id = i+1,
            .bounds = {r.x, r.y, r.w, r.h}, .enabled = 1};
    }
    IDECoreState* core = getCoreState();
    ide_button_scope_sync(&scope, pane, controls, ICON_COUNT, NULL, NULL, 0,
        !isRenaming() && (!core || !core->workspaceAuthoring.active));
}
static bool route(UIPane* pane, SDL_Event* event) {
    if (!pane) return false;
    prepare(pane);
    uint32_t activated = 0;
    bool consumed = ide_button_scope_event(&scope, event, &activated);
    if (activated && activated <= ICON_COUNT) CMD(commands[activated-1]);
    return consumed;
}
KitUiButtonState ide_icon_bar_button_state(UIPane* pane, int index) {
    if (!pane || index < 0 || index >= ICON_COUNT) return (KitUiButtonState){.disabled = 1};
    prepare(pane);
    return ide_button_scope_state(&scope, index+1, getActiveIcon() == index);
}
void handleIconBarKeyboardInput(UIPane* pane, SDL_Event* event) {
    if (route(pane, event) || event->type != SDL_KEYDOWN || event->key.repeat || !scope.count) return;
    SDL_Keycode key = event->key.keysym.sym;
    if (key >= SDLK_1 && key <= SDLK_7) CMD(commands[key-SDLK_1]);
}
void handleIconBarMouseInput(UIPane* pane, SDL_Event* event) { (void)route(pane, event); }
void handleIconBarScrollInput(UIPane* pane, SDL_Event* event) { (void)pane; (void)event; }
void handleIconBarHoverInput(UIPane* pane, int x, int y) {
    SDL_Event e = {0}; e.type = SDL_MOUSEMOTION; e.motion.x = x; e.motion.y = y;
    (void)route(pane, &e);
}
UIPaneInputHandler iconBarInputHandler = {.onKeyboard = handleIconBarKeyboardInput,
    .onMouse = handleIconBarMouseInput, .onScroll = handleIconBarScrollInput, .onHover = handleIconBarHoverInput};
