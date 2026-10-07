#include "input_tool_errors.h"
#include "ide/UI/panel_button_scope.h"
#include "ide/Panes/ToolPanels/Errors/tool_errors.h"
#include "ide/UI/scroll_input_adapter.h"
#include "ide/UI/scroll_manager.h"

static bool header_event(UIPane* pane, SDL_Event* event) {
    uint32_t id=0;
    bool consumed=ide_panel_buttons_event(IDE_PANEL_ERRORS,pane,event,errors_is_search_focused(),&id);
    if(consumed && ((event->type==SDL_KEYDOWN && event->key.keysym.sym==SDLK_TAB) ||
        ((event->type==SDL_MOUSEBUTTONDOWN || event->type==SDL_MOUSEBUTTONUP) && event->button.button==SDL_BUTTON_LEFT)))
        errors_set_search_focused(ide_panel_buttons_field_focused(IDE_PANEL_ERRORS,pane));
    size_t position = 0; bool extend = false;
    if (ide_panel_text_pointer(IDE_PANEL_ERRORS, pane, event, errors_get_search_query(), &position, &extend)) {
        (void)errors_search_pointer(position, extend);
        consumed = true;
    }
    if (id && id!=IDE_PANEL_TEXT_FIELD_ID) (void)errors_activate_top_control((ErrorTopControlId)id);
    return consumed;
}

void handleErrorsKeyboardInput(UIPane* pane, SDL_Event* event) {
    if (!header_event(pane,event)) handleErrorsEvent(pane, event);
}

void handleErrorsMouseInput(UIPane* pane, SDL_Event* event) {
    // First let the scrollbar thumb consume the event if applicable.
    PaneScrollState* scroll = errors_get_scroll_state();
    SDL_Rect track = errors_get_scroll_track_rect();
    SDL_Rect thumb = errors_get_scroll_thumb_rect();
    if (ui_scroll_input_consume(scroll, event, &track, &thumb)) {
        errors_set_scroll_rects(track, thumb);
        return;
    }

    if (!header_event(pane,event)) handleErrorsEvent(pane, event);
}

void handleErrorsScrollInput(UIPane* pane, SDL_Event* event) {
    (void)pane;
    PaneScrollState* scroll = errors_get_scroll_state();
    SDL_Rect track = errors_get_scroll_track_rect();
    SDL_Rect thumb = errors_get_scroll_thumb_rect();
    if (ui_scroll_input_consume(scroll, event, &track, &thumb)) {
        errors_set_scroll_rects(track, thumb);
        return;
    }
    if (!header_event(pane,event)) handleErrorsEvent(pane, event);
}

void handleErrorsHoverInput(UIPane* pane, int x, int y) {
    (void)pane; (void)x; (void)y;
}
