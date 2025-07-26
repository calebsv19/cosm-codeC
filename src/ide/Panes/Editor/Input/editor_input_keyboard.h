#ifndef EDITOR_INPUT_KEYBOARD_H
#define EDITOR_INPUT_KEYBOARD_H

#include <SDL2/SDL.h>
#include "ide/Panes/Editor/editor_buffer.h"
#include "ide/Panes/Editor/editor_state.h"
#include "ide/Panes/PaneInfo/pane.h"
#include "ide/Panes/Editor/editor_view.h"

void handleEditorKeyDown(SDL_Event* event, struct EditorView* view, struct UIPane* pane);
void handleArrowKeyPress(SDL_Keycode key, EditorBuffer* buffer, EditorState* state, int paneHeight);
void handleCommandShiftAction(SDL_Keycode key, EditorBuffer* buffer, EditorState* state);
bool isSpecialShiftAction(SDL_Keycode key);
void handleCommandAltAction(SDL_Keycode key, EditorBuffer* buffer, EditorState* state);
void handleCommandCharacterInput(SDL_Event* event, EditorBuffer* buffer, EditorState* state);

#endif
