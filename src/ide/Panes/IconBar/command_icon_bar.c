#include "command_icon_bar.h"
#include "core/CommandBus/command_metadata.h"
#include "ide/Panes/IconBar/icon_bar.h"
#include <stdio.h>
#include "ide/UI/ui_state.h"
#include "app/GlobalInfo/core_state.h"

void handleIconBarCommand(UIPane* pane, InputCommandMetadata meta) {
    (void)pane;
    IconTool selected;
    switch (meta.cmd) {
        case COMMAND_SELECT_ICON_PROJECT_FILES: selected = ICON_PROJECT_FILES; break;
        case COMMAND_SELECT_ICON_LIBRARIES: selected = ICON_LIBRARIES; break;
        case COMMAND_SELECT_ICON_BUILD_OUTPUT: selected = ICON_BUILD_OUTPUT; break;
        case COMMAND_SELECT_ICON_ERRORS: selected = ICON_ERRORS; break;
        case COMMAND_SELECT_ICON_ASSET_MANAGER: selected = ICON_ASSET_MANAGER; break;
        case COMMAND_SELECT_ICON_TASKS: selected = ICON_TASKS; break;
        case COMMAND_SELECT_ICON_VERSION_CONTROL: selected = ICON_VERSION_CONTROL; break;
        default:
            printf("[IconBarCommand] Unknown command: %d\n", meta.cmd);
            return;
    }
    UIState* ui = getUIState();
    bool collapse = ui->toolPanelVisible && getActiveIcon() == selected;
    setActiveIcon(selected);
    ui->toolPanelVisible = !collapse;
    requestFullRedraw(RENDER_INVALIDATION_LAYOUT | RENDER_INVALIDATION_CONTENT | RENDER_INVALIDATION_INPUT);
}


void initIconBarCommandHandler(UIPane* pane) {
    if (pane) {
        pane->handleCommand = handleIconBarCommand;
    }
}

