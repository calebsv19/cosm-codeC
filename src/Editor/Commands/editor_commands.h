#ifndef EDITOR_COMMANDS_H
#define EDITOR_COMMANDS_H

#include <SDL2/SDL.h>
#include "Editor/editor_buffer.h"
#include "Editor/editor_state.h"
#include "Editor/editor_view.h"
#include "PaneInfo/pane.h"

void handleCommandMovement(SDL_Keycode key, EditorBuffer* buffer, EditorState* state, struct 
EditorView* view, int paneHeight);
void handleCommandAction(SDL_Keycode key, EditorBuffer* buffer, EditorState* state, struct EditorView* 
view, struct UIPane* pane);
bool handleCommandNavigation(SDL_Keycode key, EditorBuffer* buffer, EditorState* state, int paneHeight, 
bool shiftHeld);

#endif

