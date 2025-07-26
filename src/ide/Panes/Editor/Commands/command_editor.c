#include "command_editor.h"
#include "ide/Panes/Editor/editor_view.h"
#include "ide/Panes/Editor/editor.h"
#include "ide/Panes/Editor/editor_text_edit.h"
#include "ide/Panes/Editor/Input/editor_input_keyboard.h"
#include "core/CommandBus/command_metadata.h"
#include "core/CommandBus/save_queue.h"
#include "core/CommandBus/command_registry.h"
#include "app/GlobalInfo/core_state.h"

#include <stdio.h>


void handleEditorCommand(UIPane* pane, InputCommandMetadata meta) {
    if (!pane || !pane->editorView) return;

    EditorView* view = pane->editorView;

    printf("[EditorCommand] Handling: %s\n", getCommandName(meta.cmd));

    switch (meta.cmd) {
        // === Basic File Ops ===
        case COMMAND_SAVE_FILE:
            if (view->fileCount > 0 && view->activeTab >= 0 && view->activeTab < view->fileCount) {
                OpenFile* file = view->openFiles[view->activeTab];
                if (file) enqueueSave(file);
            }
            break;

        // === Insert / Delete ===
        case COMMAND_INSERT_NEWLINE:
	    handleReturnKey(editorBuffer, editorState);
            break;

        case COMMAND_DELETE:
            deleteCharAtCursor();
            break;

        // === Clipboard Ops ===
        case COMMAND_CUT:
            cutSelection(view);
            break;

        case COMMAND_COPY:
            copySelection(view);
            break;

        case COMMAND_PASTE:
            pasteClipboard(view);
            break;

        case COMMAND_SELECT_ALL:
            selectAllText(view);
            break;

        // === Undo / Redo ===
        case COMMAND_UNDO:
        case COMMAND_REDO:
            handleCommandAltAction(meta.cmd == COMMAND_UNDO ? SDLK_MINUS : SDLK_EQUALS,
                                   editorBuffer, editorState);
            break;

        // === Navigation ===
        case COMMAND_GO_TO_LINE:

        // === Tab Switching / View Commands (optional future) ===
        case COMMAND_SWITCH_TAB:
        case COMMAND_SPLIT_VIEW_VERTICAL:
        case COMMAND_SPLIT_VIEW_HORIZONTAL:
            printf("[EditorCommand] Tab/View management not implemented yet: %s\n", getCommandName(meta.cmd));
            break;

        default:
            printf("[EditorCommand] Unhandled command: %s\n", getCommandName(meta.cmd));
            break;
    }
}



void initEditorCommandHandler(UIPane* pane) {
    if (!pane) return;
    pane->handleCommand = handleEditorCommand;
}

