// input_project.h
#ifndef INPUT_PROJECT_H
#define INPUT_PROJECT_H

#include "ide/Panes/PaneInfo/pane.h"
#include <SDL2/SDL.h>

void handleProjectFilesKeyboardInput(UIPane* pane, SDL_Event* event);
void handleProjectFilesMouseInput(UIPane* pane, SDL_Event* event);
void handleProjectFilesScrollInput(UIPane* pane, SDL_Event* event);
void handleProjectFilesHoverInput(UIPane* pane, int x, int y);

#endif

