#include "input_tool_errors.h"
#include "ide/Panes/ToolPanels/Errors/tool_errors.h"

void handleErrorsKeyboardInput(UIPane* pane, SDL_Event* event) {
    handleErrorsEvent(pane, event);
}

void handleErrorsMouseInput(UIPane* pane, SDL_Event* event) {
    handleErrorsEvent(pane, event);
}

void handleErrorsScrollInput(UIPane* pane, SDL_Event* event) {
    (void)pane; (void)event;
}

void handleErrorsHoverInput(UIPane* pane, int x, int y) {
    (void)pane; (void)x; (void)y;
}
