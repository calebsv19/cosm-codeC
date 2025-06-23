#include "ToolPanels/Project/command_tool_project.h"
#include "ToolPanels/Project/tool_project.h"

#include "CommandBus/command_metadata.h"
#include "CommandBus/command_registry.h"

#include "GlobalInfo/project.h"
#include "GlobalInfo/core_state.h"
#include "Editor/editor_view.h"
#include "FileIO/file_ops.h"
#include <stdio.h>


void handleProjectFilesCommand(UIPane* pane, InputCommandMetadata meta) {
    printf("[ProjectPanelCommand] Received: %s\n", getCommandName(meta.cmd));

    IDECoreState* core = getCoreState();

    switch (meta.cmd) {
	case COMMAND_OPEN_FILE:
	    if (selectedEntry && selectedEntry->type == ENTRY_FILE && selectedEntry->path) {
	        printf("[ProjectPanelCommand] Opening: %s\n", selectedEntry->path);
	        openFileInView(core->activeEditorView, selectedEntry->path);
	    } else {
	        printf("[ProjectPanelCommand] No valid file selected.\n");
	    }
	    break;


        case COMMAND_RENAME_FILE:
            if (selectedEntry && selectedEntry->type == ENTRY_FILE) {
                renamingEntry = selectedEntry;
                strncpy(renameBuffer, selectedEntry->name, sizeof(renameBuffer));
                renameBuffer[sizeof(renameBuffer) - 1] = '\0';
                printf("[ProjectPanelCommand] Entering rename mode: %s\n", renameBuffer);
            }
            break;

        case COMMAND_CONFIRM_RENAME:
            if (renamingEntry && renamingEntry->parent) {
                char oldPath[1024];
                char newPath[1024];
                snprintf(oldPath, sizeof(oldPath), "%s", renamingEntry->path);
                snprintf(newPath, sizeof(newPath), "%s/%s", renamingEntry->parent->path, renameBuffer);

                if (renameFileOnDisk(oldPath, newPath)) {
                    strncpy(newlyCreatedPath, newPath, sizeof(newlyCreatedPath));
                    newlyCreatedPath[sizeof(newlyCreatedPath) - 1] = '\0';
                }

                renamingEntry = NULL;
		pendingProjectRefresh = true;
            }
            break;

        case COMMAND_CANCEL_RENAME:
            renamingEntry = NULL;
            break;

        case COMMAND_NEW_FILE: {
            DirEntry* target = getCurrentTargetDirectory();
            createFileInProject(target, "new_file.c");
 	    pendingProjectRefresh = true;
            break;
        }

        case COMMAND_NEW_FOLDER: {
            DirEntry* target = getCurrentTargetDirectory();
            createFolderInProject(target, "new_folder");
	    pendingProjectRefresh = true;
            break;
        }

        case COMMAND_DELETE_ENTRY:
            deleteSelectedEntry();
	    pendingProjectRefresh = true;
            break;

        default:
            printf("[ProjectPanelCommand] Unhandled command: %s\n", getCommandName(meta.cmd));
            break;
    }
}

