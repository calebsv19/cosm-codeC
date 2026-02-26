#ifndef TOOL_BUILD_OUTPUT_H
#define TOOL_BUILD_OUTPUT_H

#include "ide/Panes/PaneInfo/pane.h"
#include <SDL2/SDL.h>

void handleBuildOutputEvent(UIPane* pane, SDL_Event* event);
void build_output_select_all_visible(void);
bool build_output_copy_selection_to_clipboard(void);

#endif
