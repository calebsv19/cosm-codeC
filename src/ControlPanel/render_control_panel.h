#ifndef RENDER_CONTROL_PANEL_H
#define RENDER_CONTROL_PANEL_H

#include "PaneInfo/pane.h"
#include "Render/render_pipeline.h"
#include <SDL2/SDL.h>

struct IDECoreState;

void renderControlPanelContents(UIPane* pane, bool hovered, struct IDECoreState* core);

#endif

