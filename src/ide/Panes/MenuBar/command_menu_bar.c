#include "command_menu_bar.h"
#include "core/CommandBus/command_metadata.h"
#include "core/CommandBus/save_queue.h"

#include "core/BuildSystem/build_system.h"
#include "core/BuildSystem/run_build.h"

#include "app/GlobalInfo/project.h" 
#include "app/GlobalInfo/core_state.h"
#include "ide/Panes/Editor/editor_view.h"
#include "ide/UI/ui_state.h"
#include <stdio.h>

void handleMenuBarCommand(UIPane* pane, InputCommandMetadata meta) {
    IDECoreState* core = getCoreState();
    EditorView* view = core->activeEditorView;

    switch (meta.cmd) {
        case COMMAND_BUILD_PROJECT:
	    triggerBuild();
            break;

        case COMMAND_RUN_EXECUTABLE: {
	    char exePath[1024];
	    snprintf(exePath, sizeof(exePath),
	             "%s/include/BuildOutputs/last_build", projectRootPath);
	    runExecutableAndStreamOutput(exePath);
	    break;
	}

        case COMMAND_DEBUG_EXECUTABLE:
            printf("[MenuBarCommand] Debug\n");
            break;

	case COMMAND_TOGGLE_CONTROL_PANEL: {
	    UIState* ui = getUIState();
	    ui->controlPanelVisible = !ui->controlPanelVisible;
	    printf("[MenuBarCommand] Toggle Control Panel: %s\n",
	           ui->controlPanelVisible ? "VISIBLE" : "HIDDEN");
	    break;
	}
	    	    

        case COMMAND_OPEN_BUILD_LOG:     // or new Command
        {
            UIState* ui = getUIState();
            ui->controlPanelVisible = !ui->controlPanelVisible;
            printf("[MenuBarCommand] Toggle Control Panel: %s\n",
                   ui->controlPanelVisible ? "VISIBLE" : "HIDDEN");
            break;
        }

        case COMMAND_OPEN_FILE:
            printf("[MenuBarCommand] Load (unimplemented)\n");
            break;

        case COMMAND_SAVE_FILE:
            if (view && view->type == VIEW_LEAF &&
                view->activeTab >= 0 && view->activeTab < view->fileCount) {

                OpenFile* file = view->openFiles[view->activeTab];
                if (file && file->isModified) {
                    enqueueSave(file);
                    printf("[MenuBarCommand] Save queued: %s\n", file->filePath);
                } else {
                    printf("[MenuBarCommand] Save skipped — not modified\n");
                }
            }
            break;

        default:
            printf("[MenuBarCommand] Unhandled: %d\n", meta.cmd);
            break;
    }
}



void initMenuBarCommandHandler(UIPane* pane) {
    if (pane) {
        pane->handleCommand = handleMenuBarCommand;
    }
}

