#ifndef EDITOR_INPUT_KEYBOARD_H
#define EDITOR_INPUT_KEYBOARD_H

#include <SDL2/SDL.h>
#include "Editor/editor_buffer.h"
#include "Editor/editor_state.h"
#include "PaneInfo/pane.h"
#include "Editor/editor_view.h"

void handleEditorKeyDown(SDL_Event* event, struct EditorView* view, struct UIPane* pane);
void handleArrowKeyPress(SDL_Keycode key, EditorBuffer* buffer, EditorState* state, int paneHeight);
void handleCommandShiftAction(SDL_Keycode key, EditorBuffer* buffer, EditorState* state);
bool isSpecialShiftAction(SDL_Keycode key);
void handleCommandAltAction(SDL_Keycode key, EditorBuffer* buffer, EditorState* state);
void handleCommandCharacterInput(SDL_Event* event, EditorBuffer* buffer, EditorState* state);

#endif
