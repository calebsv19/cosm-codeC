#include "input_global.h"
#include "ide/UI/ui_state.h"
#include "ide/UI/layout.h"
#include "app/GlobalInfo/core_state.h"
#include "input_hover.h"
#include "ide/Panes/ToolPanels/Project/tool_project.h"
#include "ide/Panes/Terminal/terminal.h"
#include "ide/Panes/PaneInfo/pane.h"
#include <stdio.h>

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

        case SDL_DROPFILE: {
            char* droppedPath = event->drop.file;
            IDECoreState* core = getCoreState();
            UIPane* focused = core ? core->focusedPane : NULL;
            if (droppedPath && focused && focused->role == PANE_ROLE_TERMINAL) {
                terminal_handle_dropped_path(droppedPath);
            } else if (droppedPath) {
                // Scaffold only: non-terminal drop routing can be added later.
                printf("[Drop] Ignored (focus terminal to send path): %s\n", droppedPath);
            }
            if (droppedPath) SDL_free(droppedPath);
            break;
        }

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
