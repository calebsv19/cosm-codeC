#ifndef SYSTEM_CONTROL_H
#define SYSTEM_CONTROL_H

#include <SDL2/SDL.h>
#include "PaneInfo/pane.h"

int STARTING_WIDTH, STARTING_HEIGHT;

bool initializeSystem();
void shutdownSystem(UIPane** panes, int paneCount);



#endif

