#include "ide/Panes/ToolPanels/Libraries/input_tool_libraries.h"
#include "ide/Panes/ToolPanels/Libraries/tool_libraries.h"
#include "ide/UI/scroll_manager.h"
#include "core/Analysis/library_index.h"
#include "engine/Render/render_pipeline.h"

void handleLibrariesKeyboardInput(UIPane* pane, SDL_Event* event) {
    (void)pane;
    if (!event || event->type != SDL_KEYDOWN) return;
    SDL_Keycode key = event->key.keysym.sym;
    SDL_Keymod mod = SDL_GetModState();
    bool copyCombo = (key == SDLK_c) && ((mod & KMOD_CTRL) || (mod & KMOD_GUI));
    if (copyCombo) {
        copy_selected_rows();
    }
}


void handleLibrariesMouseInput(UIPane* pane, SDL_Event* event) {
    if (!pane || !event) return;
    LibraryPanelState* st = &g_libraryPanelState;

    if (event->type == SDL_MOUSEWHEEL) {
        if (scroll_state_handle_mouse_wheel(&st->scroll, event)) {
            st->scrollThumb = scroll_state_thumb_rect(&st->scroll,
                                                      st->scrollTrack.x,
                                                      st->scrollTrack.y,
                                                      st->scrollTrack.w,
                                                      st->scrollTrack.h);
        }
        return;
    }

    if (scroll_state_handle_mouse_drag(&st->scroll, event, &st->scrollTrack, &st->scrollThumb)) {
        st->scrollThumb = scroll_state_thumb_rect(&st->scroll,
                                                  st->scrollTrack.x,
                                                  st->scrollTrack.y,
                                                  st->scrollTrack.w,
                                                  st->scrollTrack.h);
        return;
    }

    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
        if (handleLibraryHeaderClick(pane, event->button.x, event->button.y)) {
            return;
        }
        handleLibraryEntryClick(pane, event->button.x, event->button.y);
    } else if (event->type == SDL_MOUSEMOTION) {
        updateLibraryDragSelection(pane, event->motion.y);
    } else if (event->type == SDL_MOUSEBUTTONUP && event->button.button == SDL_BUTTON_LEFT) {
        endLibrarySelectionDrag();
    }
}

void handleLibrariesScrollInput(UIPane* pane, SDL_Event* event) {
    if (!pane || !event || event->type != SDL_MOUSEWHEEL) return;
    LibraryPanelState* st = &g_libraryPanelState;
    if (scroll_state_handle_mouse_wheel(&st->scroll, event)) {
        st->scrollThumb = scroll_state_thumb_rect(&st->scroll,
                                                  st->scrollTrack.x,
                                                  st->scrollTrack.y,
                                                  st->scrollTrack.w,
                                                  st->scrollTrack.h);
    }
}

void handleLibrariesHoverInput(UIPane* pane, int x, int y) {
    (void)pane;
    updateHoveredLibraryMousePosition(x, y);
}
