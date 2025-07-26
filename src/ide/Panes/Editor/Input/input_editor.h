#ifndef INPUT_EDITOR_H
#define INPUT_EDITOR_H

#include <SDL2/SDL.h>
#include "ide/Panes/PaneInfo/pane.h"

struct UIPane;
struct UIPaneInputHandler;

// Modern per-pane routing
void handleEditorKeyboardInput(struct UIPane* pane, SDL_Event* event);
void handleEditorTextInput(struct UIPane* pane, SDL_Event* event);

void handleEditorMouseInput(struct UIPane* pane, SDL_Event* event);
void handleEditorScrollInput(struct UIPane* pane, SDL_Event* event);
void handleEditorHoverInput(struct UIPane* pane, int x, int y);

// Input handler instance for pane->inputHandler
extern UIPaneInputHandler editorInputHandler;

#endif // INPUT_EDITOR_H

