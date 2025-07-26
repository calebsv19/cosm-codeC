#include "input_tool_build_output.h"
#include "ide/Panes/ToolPanels/BuildOutput/tool_build_output.h"

void handleBuildOutputKeyboardInput(UIPane* pane, SDL_Event* event) {
    (void)pane; (void)event;
}

void handleBuildOutputMouseInput(UIPane* pane, SDL_Event* event) {
    handleBuildOutputEvent(pane, event);
}

void handleBuildOutputScrollInput(UIPane* pane, SDL_Event* event) {
    (void)pane; (void)event;
}

void handleBuildOutputHoverInput(UIPane* pane, int x, int y) {
    (void)pane; (void)x; (void)y;
}

