#include "ide/Panes/ToolPanels/Libraries/command_tool_libraries.h"
#include "core/CommandBus/command_metadata.h"
#include "ide/Panes/ToolPanels/Libraries/tool_libraries.h"
#include "app/GlobalInfo/core_state.h"
#include "app/GlobalInfo/project.h"
#include "core/Analysis/library_index.h"
#include "ide/Panes/Editor/editor_view.h"
#include <stdio.h>

void handleLibrariesCommand(UIPane* pane, InputCommandMetadata meta) {
    switch (meta.cmd) {
        case COMMAND_REFRESH_LIBRARY:
            printf("[LibraryPanelCommand] Refreshing library index...\n");
            library_index_build_workspace(projectPath);
            rebuildLibraryFlatRows();
            break;

        default:
            printf("[LibraryPanelCommand] Unhandled command: %d\n", meta.cmd);
            break;
    }
}
