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

    OpenFile* activeFile = getActiveOpenFile(view);
    EditorBuffer* buffer = activeFile ? activeFile->buffer : NULL;
    EditorState* state = activeFile ? &activeFile->state : NULL;

    switch (meta.cmd) {
        // === Basic File Ops ===
        case COMMAND_SAVE_FILE:
            if (activeFile) {
                enqueueSave(activeFile);
            }
            break;

        // === Insert / Delete ===
        case COMMAND_INSERT_NEWLINE:
            if (buffer && state) {
	        handleReturnKey(buffer, state);
            }
            break;

        case COMMAND_DELETE:
            if (buffer && state) {
                deleteCharAtCursor();
            }
            break;

        // === Clipboard Ops ===
        case COMMAND_CUT:
            if (view) cutSelection(view);
            break;

        case COMMAND_COPY:
            if (view) copySelection(view);
            break;

        case COMMAND_PASTE:
            if (view) pasteClipboard(view);
            break;

        case COMMAND_SELECT_ALL:
            if (view) selectAllText(view);
            break;

        // === Undo / Redo ===
        case COMMAND_UNDO:
        case COMMAND_REDO:
            if (buffer && state) {
                handleCommandAltAction(meta.cmd == COMMAND_UNDO ? SDLK_MINUS : SDLK_EQUALS,
                                       buffer, state);
            }
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
            break;
    }
}



void initEditorCommandHandler(UIPane* pane) {
    if (!pane) return;
    pane->handleCommand = handleEditorCommand;
}
