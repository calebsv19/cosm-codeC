#include "ToolPanels/Libraries/command_tool_libraries.h"
#include "CommandBus/command_metadata.h"
#include "ToolPanels/Libraries/tool_libraries.h"
#include "GlobalInfo/core_state.h"
#include "Editor/editor_view.h"
#include <stdio.h>

void handleLibrariesCommand(UIPane* pane, InputCommandMetadata meta) {
    IDECoreState* core = getCoreState();

    switch (meta.cmd) {
        case COMMAND_OPEN_FILE:
            if (selectedLibraryEntry && selectedLibraryEntry->type == ENTRY_FILE) {
                printf("[LibraryPanelCommand] Opening: %s\n", selectedLibraryEntry->path);
                openFileInView(core->activeEditorView, selectedLibraryEntry->path);
            } else {
                printf("[LibraryPanelCommand] No valid file selected.\n");
            }
            break;

        case COMMAND_RENAME_FILE:
            printf("[LibraryPanelCommand] Rename not yet implemented.\n");
            break;

        default:
            printf("[LibraryPanelCommand] Unhandled command: %d\n", meta.cmd);
            break;
    }
}

