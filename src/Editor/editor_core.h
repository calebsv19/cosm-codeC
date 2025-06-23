#ifndef EDITOR_CORE_H
#define EDITOR_CORE_H

#include "Editor/editor_state.h"
#include "Editor/editor_buffer.h"
#include "PaneInfo/pane.h"



bool isCursorInSelection(EditorState* state);
bool isLineWhitespaceOnly(const char* line);
bool isLineInSelection(int row, int* startCol, int* endCol, EditorBuffer* buffer, EditorState* state);
int getVisibleEditorLineCount(struct UIPane* pane);

#endif
