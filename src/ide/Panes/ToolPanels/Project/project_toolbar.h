#ifndef IDE_PROJECT_TOOLBAR_H
#define IDE_PROJECT_TOOLBAR_H
#include "ide/UI/button_scope.h"
SDL_Rect ide_project_toolbar_rect(UIPane* pane, int index);
bool ide_project_toolbar_event(UIPane* pane, SDL_Event* event);
KitUiButtonState ide_project_toolbar_state(UIPane* pane, int index);
#endif
