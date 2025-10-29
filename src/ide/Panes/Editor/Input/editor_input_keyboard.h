#ifndef EDITOR_INPUT_KEYBOARD_H
#define EDITOR_INPUT_KEYBOARD_H

#include <SDL2/SDL.h>
#include "ide/Panes/Editor/editor_buffer.h"
#include "ide/Panes/Editor/editor_state.h"
#include "ide/Panes/PaneInfo/pane.h"
#include "ide/Panes/Editor/editor_view.h"
#include "ide/Panes/Editor/Commands/editor_command_payloads.h"

void editorProcessKeyCommand(struct UIPane* pane, EditorKeyCommandPayload* payload);
void editorProcessTextInput(struct UIPane* pane, const EditorTextInputPayload* payload);

#endif
