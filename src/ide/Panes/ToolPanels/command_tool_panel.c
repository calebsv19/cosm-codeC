#include "command_tool_panel.h"
#include "ide/Panes/panel_view_adapter.h"
#include "ide/Panes/ToolPanels/tool_panel_adapter.h"
#include "core/CommandBus/command_metadata.h"
#include "ide/Panes/IconBar/icon_bar.h"

#include <stdio.h>



void initToolPanelCommandHandler(UIPane* pane) {
    if (pane) {
        pane->handleCommand = handleToolPanelCommand;
    }
}



void handleToolPanelCommand(UIPane* pane, InputCommandMetadata meta) {
    const UIPanelViewAdapter* adapter = tool_panel_active_adapter();
    if (!adapter) {
        printf("[ToolPanelCommand] Unknown icon tool: %d\n", getActiveIcon());
        return;
    }
    UIPane* previousPane = tool_panel_bind_dispatch_pane(pane);
    ui_panel_view_adapter_command(adapter, pane, meta);
    tool_panel_restore_dispatch_pane(previousPane);
}
