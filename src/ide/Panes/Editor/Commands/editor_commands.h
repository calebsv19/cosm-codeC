#ifndef EDITOR_COMMANDS_H
#define EDITOR_COMMANDS_H

#include <SDL2/SDL.h>
#include "ide/Panes/Editor/editor_buffer.h"
#include "ide/Panes/Editor/editor_state.h"
#include "ide/Panes/Editor/editor_view.h"
#include "ide/Panes/PaneInfo/pane.h"

void handleCommandMovement(SDL_Keycode key, EditorBuffer* buffer, EditorState* state, int paneHeight, SDL_Keymod mod);
void handleCommandAction(SDL_Keycode key, EditorBuffer* buffer, EditorState* state, struct EditorView* view, struct UIPane* pane, SDL_Keymod mod);
bool handleCommandNavigation(SDL_Keycode key, EditorBuffer* buffer, EditorState* state, int paneHeight, bool shiftHeld);

bool editor_jump_to(EditorView* view, const char* filePath, int line, int column);

#endif
