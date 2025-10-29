#include "command_editor.h"
#include "ide/Panes/Editor/editor_view.h"
#include "ide/Panes/Editor/editor.h"
#include "ide/Panes/Editor/editor_text_edit.h"
#include "ide/Panes/Editor/Input/editor_input_keyboard.h"
#include "ide/Panes/Editor/Commands/editor_command_payloads.h"
#include "ide/Panes/Editor/undo_stack.h"
#include "core/CommandBus/command_metadata.h"
#include "core/CommandBus/save_queue.h"
#include "core/CommandBus/command_registry.h"
#include "app/GlobalInfo/core_state.h"

#include <stdio.h>
#include <stdlib.h>


void handleEditorCommand(UIPane* pane, InputCommandMetadata meta) {
    if (!pane) return;

    IDECoreState* core = getCoreState();
    EditorView* view = core ? core->activeEditorView : NULL;
    if (!view && pane->editorView) {
        view = pane->editorView;
    }

    OpenFile* activeFile = view ? getActiveOpenFile(view) : NULL;
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
            if (activeFile && buffer && state) {
                handleCommandInsertNewline(activeFile, buffer, state);
            }
            break;

        case COMMAND_DELETE:
            if (activeFile && buffer && state) {
                handleCommandDeleteCharacter(activeFile, buffer, state);
            }
            break;

        case COMMAND_DELETE_FORWARD:
            if (activeFile && buffer && state) {
                handleCommandDeleteForward(activeFile, buffer, state);
            }
            break;

        case COMMAND_INSERT_TAB:
            if (activeFile && buffer && state) {
                for (int i = 0; i < 4; ++i) {
                    handleCommandInsertCharacter(activeFile, buffer, state, ' ');
                }
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
            if (activeFile && performUndo(activeFile)) {
                printf("[Undo] Performed.\n");
            }
            break;

        case COMMAND_REDO:
            if (activeFile && performRedo(activeFile)) {
                printf("[Redo] Performed.\n");
            }
            break;

        case COMMAND_EDITOR_KEYDOWN:
            if (meta.payload) {
                editorProcessKeyCommand(pane, (EditorKeyCommandPayload*)meta.payload);
                free(meta.payload);
            }
            break;

        case COMMAND_EDITOR_TEXT_INPUT:
            if (meta.payload) {
                editorProcessTextInput(pane, (EditorTextInputPayload*)meta.payload);
                free(meta.payload);
            }
            break;

        // === Navigation ===
        case COMMAND_GO_TO_LINE:

        // === Tab Switching / View Commands (optional future) ===
        case COMMAND_SWITCH_TAB:
            if (view) {
                SDL_Keymod keyMod = meta.keyMod;
                int direction = (keyMod & KMOD_SHIFT) ? -1 : 1;
                switchTab(view, direction);
            }
            break;
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
