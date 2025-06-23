#ifndef LAYOUT_H
#define LAYOUT_H

#include "PaneInfo/pane.h"
#include "Editor/editor_view.h"
#include <SDL2/SDL.h>

struct RenderContext;

void layout_static_panes(UIPane* panes[], int* paneCount);

#endif

