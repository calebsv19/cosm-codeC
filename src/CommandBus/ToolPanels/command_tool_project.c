#include "command_tool_project.h"
#include "CommandBus/command_metadata.h"
#include "ToolPanels/tool_project.h"
#include "GlobalInfo/core_state.h"
#include "Editor/editor_view.h"
#include <stdio.h>

#include "command_tool_project.h"
#include "CommandBus/command_metadata.h"
#include "CommandBus/command_registry.h"
#include "ToolPanels/tool_project.h"
#include "GlobalInfo/core_state.h"
#include "Editor/editor_view.h"
#include <stdio.h>

void handleProjectFilesCommand(UIPane* pane, InputCommandMetadata meta) {
    printf("[ProjectPanelCommand] Received: %s\n", getCommandName(meta.cmd));

    IDECoreState* core = getCoreState();

    switch (meta.cmd) {
        case COMMAND_OPEN_FILE:
            if (selectedEntry && selectedEntry->type == ENTRY_FILE) {
                printf("[ProjectPanelCommand] Opening: %s\n", selectedEntry->path);
                openFileInView(core->activeEditorView, selectedEntry->path);
            } else {
                printf("[ProjectPanelCommand] No valid file selected.\n");
            }
            break;

        case COMMAND_RENAME_FILE:
            printf("[ProjectPanelCommand] Rename not implemented yet.\n");
            break;

        case COMMAND_NEW_FILE:
            printf("[ProjectPanelCommand] Create new file not implemented yet.\n");
            break;

        default:
            printf("[ProjectPanelCommand] Unhandled command: %s\n", getCommandName(meta.cmd));
            break;
    }
}

