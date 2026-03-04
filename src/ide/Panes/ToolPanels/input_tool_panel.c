
#include "input_tool_panel.h"
#include "ide/Panes/panel_view_adapter.h"
#include "ide/Panes/ToolPanels/tool_panel_adapter.h"
#include "ide/Panes/ToolPanels/command_tool_panel.h"



// ==== Keyboard ====
void handleToolPanelKeyboardInput(UIPane* pane, SDL_Event* event) {
    UIPane* previousPane = tool_panel_bind_dispatch_pane(pane);
    ui_panel_view_adapter_keyboard(tool_panel_active_adapter(), pane, event);
    tool_panel_restore_dispatch_pane(previousPane);
}


// ==== Mouse ====
void handleToolPanelMouseInput(UIPane* pane, SDL_Event* event) {
    UIPane* previousPane = tool_panel_bind_dispatch_pane(pane);
    ui_panel_view_adapter_mouse(tool_panel_active_adapter(), pane, event);
    tool_panel_restore_dispatch_pane(previousPane);
}


// ==== Scroll ====
void handleToolPanelScrollInput(UIPane* pane, SDL_Event* event) {
    UIPane* previousPane = tool_panel_bind_dispatch_pane(pane);
    ui_panel_view_adapter_scroll(tool_panel_active_adapter(), pane, event);
    tool_panel_restore_dispatch_pane(previousPane);
}

// ==== Hover ====
void handleToolPanelHoverInput(UIPane* pane, int x, int y) {
    UIPane* previousPane = tool_panel_bind_dispatch_pane(pane);
    ui_panel_view_adapter_hover(tool_panel_active_adapter(), pane, x, y);
    tool_panel_restore_dispatch_pane(previousPane);
}

// ==== Text Input ====
void handleToolPanelTextInput(UIPane* pane, SDL_Event* event) {
    UIPane* previousPane = tool_panel_bind_dispatch_pane(pane);
    ui_panel_view_adapter_text_input(tool_panel_active_adapter(), pane, event);
    tool_panel_restore_dispatch_pane(previousPane);
}


// ==== Handler Struct Export ====
UIPaneInputHandler toolPanelInputHandler = {
    .onCommand = handleToolPanelCommand,
    .onKeyboard = handleToolPanelKeyboardInput,
    .onMouse = handleToolPanelMouseInput,
    .onScroll = handleToolPanelScrollInput,
    .onHover = handleToolPanelHoverInput,
    .onTextInput = handleToolPanelTextInput,
};
