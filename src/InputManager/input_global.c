#include "input_global.h"
#include "../UI/ui_state.h"
#include "../UI/layout.h"
#include "input_hover.h"

void handleWindowGlobalEvents(SDL_Event* event,
                              UIPane** panes, int* paneCountRef,
                              bool* running) {
    switch (event->type) {
        case SDL_QUIT:
            *running = false;
            break;

        case SDL_WINDOWEVENT:
            handleWindowEvent(event);
            break;

        case SDL_MOUSEMOTION:
            handleHoverUpdate(event, panes, *paneCountRef);
            break;

        default:
            break;
    }
}

void handleWindowEvent(SDL_Event* event) {
    switch (event->window.event) {
        case SDL_WINDOWEVENT_LEAVE:
            getCoreState()->activeMousePane = NULL;
            break;

        case SDL_WINDOWEVENT_FOCUS_GAINED:
            printf("[Global] Window gained focus.\n");
            break;

        case SDL_WINDOWEVENT_FOCUS_LOST:
            printf("[Global] Window lost focus.\n");
            break;
    }
}

