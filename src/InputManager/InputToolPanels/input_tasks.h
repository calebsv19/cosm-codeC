#ifndef INPUT_TASKS_H
#define INPUT_TASKS_H

#include "pane.h"
#include <SDL2/SDL.h>

void handleTasksKeyboardInput(UIPane* pane, SDL_Event* event);
void handleTasksMouseInput(UIPane* pane, SDL_Event* event);
void handleTasksScrollInput(UIPane* pane, SDL_Event* event);
void handleTasksHoverInput(UIPane* pane, int x, int y);

#endif // INPUT_TASKS_H

