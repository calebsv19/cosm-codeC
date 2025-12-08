#include "input_global.h"
#include "ide/UI/ui_state.h"
#include "ide/UI/layout.h"
#include "app/GlobalInfo/core_state.h"
#include "input_hover.h"
#include "ide/Panes/ToolPanels/Project/tool_project.h"

void handleWindowGlobalEvents(SDL_Event* event,
                              UIPane** panes, int* paneCountRef,
                              bool* running) {
    switch (event->type) {
        case SDL_QUIT:
            *running = false;
            break;

        case SDL_APP_TERMINATING:
            *running = false;
            break;

        case SDL_WINDOWEVENT:
            if (event->window.event == SDL_WINDOWEVENT_CLOSE) {
                *running = false;
                SDL_Event quit = {0};
                quit.type = SDL_QUIT;
                SDL_PushEvent(&quit);
            } else {
                handleWindowEvent(event);
            }
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
            resetProjectDragState();
            break;

        case SDL_WINDOWEVENT_FOCUS_GAINED:
            printf("[Global] Window gained focus.\n");
            break;

        case SDL_WINDOWEVENT_FOCUS_LOST:
            printf("[Global] Window lost focus.\n");
            break;
    }
}
