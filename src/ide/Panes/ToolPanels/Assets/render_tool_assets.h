#ifndef RENDER_TOOL_ASSETS_H
#define RENDER_TOOL_ASSETS_H

#include "ide/Panes/PaneInfo/pane.h"
#include "ide/UI/scroll_manager.h"

// Responsible for drawing the Asset Manager tool panel
void renderAssetManagerPanel(UIPane* pane);
PaneScrollState* assets_get_scroll_state(UIPane* pane);
SDL_Rect assets_get_scroll_track_rect(void);
SDL_Rect assets_get_scroll_thumb_rect(void);
SDL_Rect assets_get_open_all_rect(void);
SDL_Rect assets_get_close_all_rect(void);

#endif
