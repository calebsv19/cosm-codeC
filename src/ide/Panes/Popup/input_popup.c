#include "input_popup.h"
#include <stdio.h>

#include "core/InputManager/UserInput/rename_flow.h"

void handlePopupKeyboardInput(UIPane* pane, SDL_Event* event) {
    (void)pane;
    if (event->type == SDL_KEYDOWN) {
        SDL_Keycode key = event->key.keysym.sym;

        if (isRenaming()) {
            if (key == SDLK_RETURN) {
                submitRename();
                return;
            } else if (key == SDLK_ESCAPE) {
                cancelRename();
                return;
            }
        }
    }
}

static void handlePopupTextInput(UIPane* pane, SDL_Event* event) {
    (void)pane;
    if (!event || event->type != SDL_TEXTINPUT) return;
    if (!isRenaming()) return;
    handleRenameTextInput(event->text.text[0]);
}

void handlePopupMouseInput(UIPane* pane, SDL_Event* event) {
    (void)pane; (void)event;
    printf("[Popup] Mouse input received.\n");
    // TODO: Detect clicks outside or on close button
}

void handlePopupScrollInput(UIPane* pane, SDL_Event* event) {
    (void)pane; (void)event;
    // Optional for scrollable popup content
}

void handlePopupHoverInput(UIPane* pane, int x, int y) {
    (void)pane; (void)x; (void)y;
    // Optional for hover highlights or previews
}

UIPaneInputHandler popupInputHandler = {
    .onCommand = NULL,
    .onKeyboard = handlePopupKeyboardInput,
    .onMouse = handlePopupMouseInput,
    .onScroll = handlePopupScrollInput,
    .onHover = handlePopupHoverInput,
    .onTextInput = handlePopupTextInput,
};
