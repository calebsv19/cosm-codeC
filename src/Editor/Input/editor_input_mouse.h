#ifndef EDITOR_INPUT_MOUSE_H
#define EDITOR_INPUT_MOUSE_H

#include "Editor/editor_view.h"
#include "PaneInfo/pane.h"
#include <SDL2/SDL.h>

void handleEditorMouseClick(struct UIPane* pane, SDL_Event* event, struct EditorView* view);
void handleEditorMouseButtonUp(struct UIPane* pane, SDL_Event* event, struct EditorView* view);
void handleEditorMouseDrag(struct UIPane* pane, SDL_Event* event, struct EditorView* view);
bool handleEditorScrollbarEvent(struct UIPane* pane, SDL_Event* event);
bool handleEditorScrollbarThumbClick(SDL_Event* event, int mx, int my);
bool handleEditorScrollWheel(UIPane* pane, SDL_Event* event);
bool handleEditorScrollbarDrag(SDL_Event* event);
bool resetCursorPositionToMouse(struct UIPane* pane, int mouseX, int mouseY, EditorBuffer* buffer, 
EditorState* state);

#endif

