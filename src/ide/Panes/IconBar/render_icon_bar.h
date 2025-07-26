#ifndef RENDER_ICON_BAR_H
#define RENDER_ICON_BAR_H

#include "ide/Panes/PaneInfo/pane.h"
#include "engine/Render/render_pipeline.h"
#include <SDL2/SDL.h>

struct IDECoreState;

void renderIconBarContents(UIPane* pane, bool hovered, struct IDECoreState* core);

#endif

