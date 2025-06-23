#ifndef RENDER_EDITOR_VIEW_H
#define RENDER_EDITOR_VIEW_H

#include "PaneInfo/pane.h"
#include "Editor/editor_view.h"
#include "Editor/editor_buffer.h"
#include "Editor/editor_state.h"
#include "GlobalInfo/core_state.h"
#include <SDL2/SDL.h>

// Function declarations
void renderEditorViewContents(UIPane* pane, bool hovered, IDECoreState* core);
void renderEditorEntry(EditorView* view);
void renderEditorViewRecursive(EditorView* view);
void renderLeafEditorView(EditorView* view);
void renderEditorScrollbar(EditorView* view, OpenFile* file);
void renderEditorBuffer(EditorBuffer* buffer, EditorState* state, int x, int y, int w, int h);

#endif // RENDER_EDITOR_VIEW_H

