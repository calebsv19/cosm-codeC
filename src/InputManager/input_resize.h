#ifndef INPUT_RESIZE_H
#define INPUT_RESIZE_H

#include <SDL2/SDL.h>
#include "../pane.h"
#include "../UI/resize.h"

// Used by input_manager.c to manage pane boundary dragging
void handleResizeDragging(SDL_Event* event,
                          ResizeZone* zones, int zoneCount,
                          UIPane** panes, int* paneCount);

#endif // INPUT_RESIZE_H

