#ifndef RENDER_TERMINAL_H
#define RENDER_TERMINAL_H

#include "ide/Panes/PaneInfo/pane.h"
#include "engine/Render/render_pipeline.h"
#include <SDL2/SDL.h>

struct IDECoreState;

void renderTerminalContents(UIPane* pane, bool hovered, struct IDECoreState* core);

#endif


