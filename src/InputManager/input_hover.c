#include "input_hover.h"
#include "GlobalInfo/core_state.h"
#include <SDL2/SDL.h>

void handleHoverUpdate(SDL_Event* event, UIPane** panes, int paneCount) {
    int mouseX = event->motion.x;
    int mouseY = event->motion.y;

    UIPane* hovered = NULL;

    for (int i = 0; i < paneCount; i++) {
        if (isPointInsidePane(panes[i], mouseX, mouseY)) {
            hovered = panes[i];

            // NEW: Dispatch to onHover()
            if (hovered->inputHandler && hovered->inputHandler->onHover) {
                hovered->inputHandler->onHover(hovered, mouseX, mouseY);
            }

            break;
        }
    }

    getCoreState()->activeMousePane = hovered;
}

