#include "input_hover.h"
#include "app/GlobalInfo/core_state.h"
#include "ide/Panes/Editor/editor_view.h"
#include "ide/Panes/Editor/editor_view_state.h"
#include <SDL2/SDL.h>

void handleHoverUpdate(SDL_Event* event, UIPane** panes, int paneCount) {
    int mouseX = 0;
    int mouseY = 0;

    switch (event->type) {
        case SDL_MOUSEMOTION:
            mouseX = event->motion.x;
            mouseY = event->motion.y;
            break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            mouseX = event->button.x;
            mouseY = event->button.y;
            break;
        case SDL_MOUSEWHEEL:
            SDL_GetMouseState(&mouseX, &mouseY);
            break;
        default:
            return;
    }

    IDECoreState* state = getCoreState();
    state->mouseX = mouseX;
    state->mouseY = mouseY;

    UIPane* hovered = NULL;
    EditorView* hoverLeaf = NULL;

    for (int i = 0; i < paneCount; i++) {
        if (isPointInsidePane(panes[i], mouseX, mouseY)) {
            hovered = panes[i];

            if (hovered->inputHandler && hovered->inputHandler->onHover) {
                hovered->inputHandler->onHover(hovered, mouseX, mouseY);
            }

            if (hovered->role == PANE_ROLE_EDITOR && hovered->editorView) {
                hoverLeaf = hitTestLeaf(state->editorViewState, mouseX, mouseY);
            }

            break;
        }
    }

    setHoveredEditorView(hoverLeaf);
    state->activeMousePane = hovered;
}
