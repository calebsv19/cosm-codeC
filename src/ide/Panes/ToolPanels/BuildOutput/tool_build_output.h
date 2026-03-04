#ifndef TOOL_BUILD_OUTPUT_H
#define TOOL_BUILD_OUTPUT_H

#include "ide/Panes/PaneInfo/pane.h"
#include "ide/UI/panel_metrics.h"
#include "ide/UI/scroll_manager.h"
#include <SDL2/SDL.h>

#define BUILD_OUTPUT_LINE_HEIGHT IDE_UI_DENSE_ROW_HEIGHT

void handleBuildOutputEvent(UIPane* pane, SDL_Event* event);
void build_output_select_all_visible(void);
bool build_output_copy_selection_to_clipboard(void);
PaneScrollState* build_output_get_scroll_state(void);
SDL_Rect build_output_get_scroll_track_rect(void);
SDL_Rect build_output_get_scroll_thumb_rect(void);
void build_output_set_scroll_rects(SDL_Rect track, SDL_Rect thumb);
int build_output_content_top(const UIPane* pane);
int build_output_first_row_y(const UIPane* pane);

#endif
