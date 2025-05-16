#ifndef RENDER_TOOL_PANEL_H
#define RENDER_TOOL_PANEL_H

#include "pane.h"
#include "../render_pipeline.h"
#include <SDL2/SDL.h>

struct IDECoreState;

void renderToolPanelContents(UIPane* pane, bool hovered, struct IDECoreState* core);


void renderToolPanelView(UIPane* pane);

#endif

