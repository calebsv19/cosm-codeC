#ifndef EDITOR_TEXT_EDIT_H
#define EDITOR_TEXT_EDIT_H

#include "Editor/editor_view.h"
#include "Editor/editor_buffer.h"
#include "Editor/editor_state.h"

void handleReturnKey(EditorBuffer* buffer, EditorState* state);
void handleBackspaceKey(EditorBuffer* buffer, EditorState* state);
void handleCommandInsertNewline(OpenFile* file, EditorBuffer* buffer, EditorState* state);
void handleCommandDeleteCharacter(OpenFile* file, EditorBuffer* buffer, EditorState* state);
void handleCommandInsertCharacter(OpenFile* file, EditorBuffer* buffer, EditorState* state, char ch);

#endif
