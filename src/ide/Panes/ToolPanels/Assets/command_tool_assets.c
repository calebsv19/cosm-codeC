#include "ide/Panes/ToolPanels/Assets/command_tool_assets.h"
#include "ide/Panes/ToolPanels/Assets/tool_assets.h"
#include "core/CommandBus/command_metadata.h"
#include <stdio.h>

void handleAssetsCommand(UIPane* pane, InputCommandMetadata meta) {
    (void)pane;

    switch (meta.cmd) {
        case COMMAND_REFRESH_ASSET_LIST:
            printf("[AssetPanelCommand] Refreshing asset list\n");
            initAssetManagerPanel();
            break;

        case COMMAND_PREVIEW_SELECTED_ASSET:
            printf("[AssetPanelCommand] Previewing selected asset (not implemented yet)\n");
            // Future: previewSelectedAsset();
            break;

        default:
            printf("[AssetPanelCommand] Unknown command: %d\n", meta.cmd);
            break;
    }
}

