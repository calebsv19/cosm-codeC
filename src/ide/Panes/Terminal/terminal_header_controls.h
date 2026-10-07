#ifndef IDE_TERMINAL_HEADER_CONTROLS_H
#define IDE_TERMINAL_HEADER_CONTROLS_H
#include "ide/UI/panel_button_scope.h"
enum { IDE_TERMINAL_HEADER_CLOSE=1, IDE_TERMINAL_HEADER_ADD=2, IDE_TERMINAL_HEADER_TAB_BASE=100 };
bool ide_terminal_header_activate(uint32_t id);
void ide_terminal_header_begin(UIPane* pane);
void ide_terminal_header_add(uint32_t id, SDL_Rect rect);
void ide_terminal_header_publish(UIPane* pane);
bool ide_terminal_header_event(UIPane* pane, SDL_Event* event, uint32_t* activated);
#endif
