#include "ide/Panes/ToolPanels/Git/command_tool_git.h"
#include "core/CommandBus/command_metadata.h"
#include "ide/Panes/ToolPanels/Git/tool_git.h"

#include <stdio.h>

void handleGitCommand(UIPane* pane, InputCommandMetadata meta) {
    (void)pane;

    switch (meta.cmd) {
        case COMMAND_REFRESH_GIT_STATUS:
            printf("[GitCommand] Refreshing Git status...\n");
            refreshGitStatus();
            resetGitTree();
            break;

        case COMMAND_STAGE_SELECTED_FILE:
            printf("[GitCommand] Staging file (not yet implemented)\n");
            break;

        case COMMAND_COMMIT_CHANGES:
            printf("[GitCommand] Committing changes (not yet implemented)\n");
            break;

        default:
            printf("[GitCommand] Unknown command: %d\n", meta.cmd);
            break;
    }
}
