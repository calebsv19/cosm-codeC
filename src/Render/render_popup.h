#ifndef RENDER_POPUP_H
#define RENDER_POPUP_H

#include "../Popup/popup_system.h"
#include "../pane.h"
#include <SDL2/SDL.h>

void renderPopupQueueContents(void);
void renderSinglePopup(Popup* popup, SDL_Renderer* renderer, int index);

#endif

