#ifndef EDITOR_CLIPBOARD_H
#define EDITOR_CLIPBOARD_H

#include "ide/Panes/Editor/editor_buffer.h"
#include "ide/Panes/Editor/editor_state.h"
#include <SDL2/SDL.h>

bool handleCommandCopySelection(EditorBuffer* buffer, EditorState* state);
bool handleCommandCutSelection(EditorBuffer* buffer, EditorState* state);
bool handleCommandPasteClipboard(EditorBuffer* buffer, EditorState* state);
bool removeSelectedText(EditorBuffer* buffer, EditorState* state);

bool handleCommandLineClipboard(SDL_Keycode key, EditorBuffer* buffer, EditorState* state);
bool handleCommandTextClipboard(SDL_Keycode key, EditorBuffer* buffer, EditorState* state);

void pushCutLine(const char* line);
const char* peekLastCutLine(void);
void clearCutBuffer(void);

#endif
