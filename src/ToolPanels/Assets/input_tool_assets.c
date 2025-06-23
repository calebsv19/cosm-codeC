#include "input_tool_assets.h"
#include "ToolPanels/Assets/tool_assets.h"

void handleAssetsKeyboardInput(UIPane* pane, SDL_Event* event) {
    (void)pane; (void)event;
}

void handleAssetsMouseInput(UIPane* pane, SDL_Event* event) {
    handleAssetManagerEvent(pane, event);
}

void handleAssetsScrollInput(UIPane* pane, SDL_Event* event) {
    (void)pane; (void)event;
}

void handleAssetsHoverInput(UIPane* pane, int x, int y) {
    (void)pane; (void)x; (void)y;
}

