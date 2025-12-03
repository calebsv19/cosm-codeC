#ifndef RENDER_TOOL_PROJECT_H
#define RENDER_TOOL_PROJECT_H

#include "ide/Panes/PaneInfo/pane.h"
#include "ide/UI/scroll_manager.h"

struct DirEntry;

// Responsible for drawing the project files tool panel
void renderProjectFilesPanel(UIPane* pane);
PaneScrollState* project_get_scroll_state(UIPane* pane);
SDL_Rect project_get_scroll_track_rect(void);
SDL_Rect project_get_scroll_thumb_rect(void);

#endif
