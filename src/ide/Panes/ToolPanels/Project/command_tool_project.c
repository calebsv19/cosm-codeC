#include "ide/Panes/ToolPanels/Project/command_tool_project.h"
#include "ide/Panes/ToolPanels/Project/tool_project.h"

#include "core/CommandBus/command_metadata.h"
#include "core/CommandBus/command_registry.h"

#include "app/GlobalInfo/project.h"
#include "app/GlobalInfo/core_state.h"
#include "core/Analysis/analysis_scheduler.h"
#include "ide/Panes/Editor/editor_view.h"
#include "core/FileIO/file_ops.h"
#include <stdio.h>


void handleProjectFilesCommand(UIPane* pane, InputCommandMetadata meta) {
    printf("[ProjectPanelCommand] Received: %s\n", getCommandName(meta.cmd));

    IDECoreState* core = getCoreState();

    switch (meta.cmd) {
	case COMMAND_OPEN_FILE:
	    if (selectedFile && selectedFile->path) {
	        printf("[ProjectPanelCommand] Opening: %s\n", selectedFile->path);
	        openFileInView(core->activeEditorView, selectedFile->path);
	    } else {
	        printf("[ProjectPanelCommand] No valid file selected.\n");
	    }
	    break;


        case COMMAND_RENAME_FILE:
            if (selectedFile) {
                project_begin_inline_rename(selectedFile);
            } else if (selectedDirectory && selectedDirectory != projectRoot) {
                project_begin_inline_rename(selectedDirectory);
            } else {
                project_end_inline_rename();
            }

            if (renamingEntry) {
                printf("[ProjectPanelCommand] Entering rename mode: %s\n", renameBuffer);
            }
            break;

        case COMMAND_CONFIRM_RENAME:
            if (renamingEntry && renamingEntry->parent) {
                char* newlyCreatedPath = project_newly_created_path_buffer();
                char oldPath[1024];
                char newPath[1024];
                snprintf(oldPath, sizeof(oldPath), "%s", renamingEntry->path);
                snprintf(newPath, sizeof(newPath), "%s/%s", renamingEntry->parent->path, renameBuffer);

                if (renameFileOnDisk(oldPath, newPath)) {
                    strncpy(newlyCreatedPath, newPath, PROJECT_PATH_BUFFER_CAP);
                    newlyCreatedPath[PROJECT_PATH_BUFFER_CAP - 1] = '\0';
                }

                project_end_inline_rename();
                queueProjectRefresh(ANALYSIS_REASON_PROJECT_MUTATION);
            }
            break;

        case COMMAND_CANCEL_RENAME:
            project_end_inline_rename();
            break;

        case COMMAND_NEW_FILE: {
            DirEntry* target = getCurrentTargetDirectory();
            createFileInProject(target, "new_file.c");
            queueProjectRefresh(ANALYSIS_REASON_PROJECT_MUTATION);
            break;
        }

        case COMMAND_NEW_FOLDER: {
            DirEntry* target = getCurrentTargetDirectory();
            createFolderInProject(target, "new_folder");
            queueProjectRefresh(ANALYSIS_REASON_PROJECT_MUTATION);
            break;
        }

        case COMMAND_DELETE_FILE:
            deleteSelectedFile();
	    break;

        case COMMAND_DELETE_FOLDER:
            deleteSelectedDirectory();
            break;

        case COMMAND_DELETE_ENTRY:
            if (selectedFile) {
                deleteSelectedFile();
            } else {
                deleteSelectedDirectory();
            }
            break;

        default:
            printf("[ProjectPanelCommand] Unhandled command: %s\n", getCommandName(meta.cmd));
            break;
    }
}
