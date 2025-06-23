#ifndef TOOL_ASSETS_H
#define TOOL_ASSETS_H

#include "PaneInfo/pane.h"
#include "GlobalInfo/project.h"
#include <SDL2/SDL.h>

extern DirEntry* assetRoot;

void initAssetManagerPanel(void);
void handleAssetManagerEvent(UIPane* pane, SDL_Event* event);

#endif

