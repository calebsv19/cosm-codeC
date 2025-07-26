#ifndef INPUT_GIT_H
#define INPUT_GIT_H

#include "ide/Panes/PaneInfo/pane.h"
#include <SDL2/SDL.h>

void handleGitKeyboardInput(UIPane* pane, SDL_Event* event);
void handleGitMouseInput(UIPane* pane, SDL_Event* event);
void handleGitScrollInput(UIPane* pane, SDL_Event* event);
void handleGitHoverInput(UIPane* pane, int x, int y);

#endif

