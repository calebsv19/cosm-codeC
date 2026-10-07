#ifndef IDE_WINDOW_LIFECYCLE_H
#define IDE_WINDOW_LIFECYCLE_H
#include "ide/Panes/PaneInfo/pane.h"
#include "kit_ui_window_sdl.h"
void ide_window_cancel_pointer_drafts(UIPane** panes, int* pane_count);
void ide_window_lifecycle_event(const SDL_Event* event, UIPane** panes, int* pane_count);
bool ide_window_lifecycle_refresh(UIPane** panes, int* pane_count);
#endif
