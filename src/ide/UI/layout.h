#ifndef LAYOUT_H
#define LAYOUT_H

#include "ide/Panes/PaneInfo/pane.h"
#include "ide/Panes/Editor/editor_view.h"
#include <SDL2/SDL.h>

struct RenderContext;

void layout_static_panes(UIPane* panes[], int* paneCount);
void ide_refresh_live_theme(void);

#endif
