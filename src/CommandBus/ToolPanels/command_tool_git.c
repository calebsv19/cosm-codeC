#include "command_tool_git.h"
#include "CommandBus/command_metadata.h"
#include "ToolPanels/tool_git.h"
#include <stdio.h>

void handleGitCommand(UIPane* pane, InputCommandMetadata meta) {
    (void)pane;

    switch (meta.cmd) {
        case COMMAND_REFRESH_GIT_STATUS:
            printf("[GitCommand] Refreshing Git status...\n");
            refreshGitStatus();
            break;

        case COMMAND_STAGE_SELECTED_FILE:
            printf("[GitCommand] Staging file (not yet implemented)\n");
            // Future: stageSelectedGitFile();
            break;

        case COMMAND_COMMIT_CHANGES:
            printf("[GitCommand] Committing changes (not yet implemented)\n");
            // Future: openCommitPopup();
            break;

        default:
            printf("[GitCommand] Unknown command: %d\n", meta.cmd);
            break;
    }
}

