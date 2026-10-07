#include "terminal_header_controls.h"
#include "terminal.h"
static UIPanelTaggedRect items[IDE_BUTTON_SCOPE_CAPACITY];
static UIPanelTaggedRectList hits={items,0,IDE_BUTTON_SCOPE_CAPACITY};
void ide_terminal_header_begin(UIPane* pane) {
    hits.count=0;
    ide_panel_buttons_domain(IDE_PANEL_TERMINAL,pane,NULL,terminal_session_generation());
}
void ide_terminal_header_add(uint32_t id, SDL_Rect rect) { (void)ui_panel_tagged_rect_list_add(&hits,id,rect); }
void ide_terminal_header_publish(UIPane* pane) {
    ide_panel_buttons_publish(IDE_PANEL_TERMINAL,pane,&hits,terminal_can_close_active_interactive() ? 0 : IDE_TERMINAL_HEADER_CLOSE);
}
bool ide_terminal_header_event(UIPane* pane, SDL_Event* event, uint32_t* activated) {
    if (!pane || !event) { if (activated) *activated=0; return false; }
    if (event->type==SDL_MOUSEBUTTONDOWN && event->button.y>=pane->y+TERMINAL_PADDING+TERMINAL_HEADER_HEIGHT) {
        if (activated) *activated=0;
        return false;
    }
    ide_panel_buttons_domain(IDE_PANEL_TERMINAL,pane,NULL,terminal_session_generation());
    /* Tab/Enter/Space/text must continue to the PTY, even after a header click. */
    return ide_panel_buttons_event(IDE_PANEL_TERMINAL,pane,event,true,activated);
}
