#include "ToolPanels/Tasks/command_tool_tasks.h"
#include "CommandBus/command_metadata.h"
#include "CommandBus/command_registry.h"
#include "ToolPanels/Tasks/tool_tasks.h"
#include <stdio.h>

void handleTasksCommand(UIPane* pane, InputCommandMetadata meta) {
    printf("[TaskPanelCommand] Received: %s\n", getCommandName(meta.cmd));

    switch (meta.cmd) {
        case COMMAND_ADD_TASK:        taskPanelAddTask(pane); break;
        case COMMAND_RENAME_TASK:     taskPanelRenameTask(pane); break;
        case COMMAND_DELETE_TASK:     taskPanelDeleteTask(pane); break;
        case COMMAND_MOVE_TASK_UP:    taskPanelMoveTaskUp(pane); break;
        case COMMAND_MOVE_TASK_DOWN:  taskPanelMoveTaskDown(pane); break;
        default:
            printf("[TaskPanelCommand] Unhandled: %s\n", getCommandName(meta.cmd));
            break;
    }
}
