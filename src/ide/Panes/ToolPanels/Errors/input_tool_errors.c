#include "input_tool_errors.h"
#include "ide/Panes/ToolPanels/Errors/tool_errors.h"
#include "ide/UI/scroll_input_adapter.h"
#include "ide/UI/scroll_manager.h"

void handleErrorsKeyboardInput(UIPane* pane, SDL_Event* event) {
    handleErrorsEvent(pane, event);
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

    handleErrorsEvent(pane, event);
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
    handleErrorsEvent(pane, event);
}

void handleErrorsHoverInput(UIPane* pane, int x, int y) {
    (void)pane; (void)x; (void)y;
}
