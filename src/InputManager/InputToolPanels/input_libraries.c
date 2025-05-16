#include "input_libraries.h"
#include "../../ToolPanels/tool_libraries.h"

void handleLibrariesKeyboardInput(UIPane* pane, SDL_Event* event) {
    // No keyboard support yet, add as needed
}


void handleLibrariesMouseInput(UIPane* pane, SDL_Event* event) {
    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
        handleLibraryEntryClick(pane, event->button.x);
    }
}

void handleLibrariesScrollInput(UIPane* pane, SDL_Event* event) {
    // Placeholder — implement if library scroll needed
    (void)pane;
    (void)event;
}

void handleLibrariesHoverInput(UIPane* pane, int x, int y) {
    updateHoveredLibraryMousePosition(x, y);
}
