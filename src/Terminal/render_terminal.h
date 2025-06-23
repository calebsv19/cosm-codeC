#ifndef RENDER_TERMINAL_H
#define RENDER_TERMINAL_H

#include "PaneInfo/pane.h"
#include "Render/render_pipeline.h"
#include <SDL2/SDL.h>

struct IDECoreState;

void renderTerminalContents(UIPane* pane, bool hovered, struct IDECoreState* core);

#endif


