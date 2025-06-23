#ifndef RENDER_ICON_BAR_H
#define RENDER_ICON_BAR_H

#include "PaneInfo/pane.h"
#include "Render/render_pipeline.h"
#include <SDL2/SDL.h>

struct IDECoreState;

void renderIconBarContents(UIPane* pane, bool hovered, struct IDECoreState* core);

#endif

