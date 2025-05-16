#include "input_popup.h"
#include <stdio.h>

void handlePopupKeyboardInput(UIPane* pane, SDL_Event* event) {
    (void)pane; (void)event;
    printf("[Popup] Keyboard input received.\n");
    // TODO: Escape key to dismiss, hotkey close, etc.
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
};

