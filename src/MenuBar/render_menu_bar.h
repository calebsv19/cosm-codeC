#ifndef RENDER_MENU_BAR_H
#define RENDER_MENU_BAR_H

#include "PaneInfo/pane.h"
#include "Render/render_pipeline.h"
#include <SDL2/SDL.h>

struct IDECoreState;

void renderMenuBarContents(UIPane* pane, struct IDECoreState* core);
void renderMenuBarStandard(UIPane* pane, SDL_Renderer* renderer, struct IDECoreState* core);
void renderMenuBarCenteredFile(UIPane* pane, SDL_Renderer* renderer, struct IDECoreState* core);



#endif

