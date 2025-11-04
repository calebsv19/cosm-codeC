#ifndef RENDER_TOOL_PROJECT_H
#define RENDER_TOOL_PROJECT_H

#include "ide/Panes/PaneInfo/pane.h"
#include "core/UI/scroll_manager.h"

struct DirEntry;

// Responsible for drawing the project files tool panel
void renderProjectFilesPanel(UIPane* pane);
PaneScrollState* project_get_scroll_state(UIPane* pane);

#endif
