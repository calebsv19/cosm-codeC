#include "ide/Panes/ToolPanels/Libraries/input_tool_libraries.h"
#include "ide/Panes/ToolPanels/Libraries/tool_libraries.h"
#include "ide/UI/input_modifiers.h"
#include "ide/UI/scroll_input_adapter.h"
#include "ide/UI/scroll_manager.h"
#include "core/Analysis/library_index.h"
#include "engine/Render/render_pipeline.h"

void handleLibrariesKeyboardInput(UIPane* pane, SDL_Event* event) {
    (void)pane;
    if (!event || event->type != SDL_KEYDOWN) return;
    SDL_Keycode key = event->key.keysym.sym;
    Uint16 mod = (Uint16)SDL_GetModState();
    bool accel = ui_input_has_primary_accel(mod);
    bool selectAllCombo = accel && (key == SDLK_a);
    bool copyCombo = accel && (key == SDLK_c);
    if (selectAllCombo) {
        select_all_library_rows();
        return;
    }
    if (copyCombo) {
        copy_selected_rows();
        return;
    }
}


void handleLibrariesMouseInput(UIPane* pane, SDL_Event* event) {
    if (!pane || !event) return;
    LibraryPanelState* st = libraries_panel_state();

    if (ui_scroll_input_consume(&st->scroll, event, &st->scrollTrack, &st->scrollThumb)) {
        return;
    }

    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
        if (handleLibraryHeaderClick(pane, event->button.x, event->button.y)) {
            return;
        }
        handleLibraryEntryClick(pane,
                                event->button.x,
                                event->button.y,
                                (Uint16)SDL_GetModState());
    } else if (event->type == SDL_MOUSEMOTION) {
        updateLibraryDragSelection(pane, event->motion.y);
    } else if (event->type == SDL_MOUSEBUTTONUP && event->button.button == SDL_BUTTON_LEFT) {
        endLibrarySelectionDrag();
    }
}

void handleLibrariesScrollInput(UIPane* pane, SDL_Event* event) {
    if (!pane || !event || event->type != SDL_MOUSEWHEEL) return;
    LibraryPanelState* st = libraries_panel_state();
    (void)ui_scroll_input_consume(&st->scroll, event, &st->scrollTrack, &st->scrollThumb);
}

void handleLibrariesHoverInput(UIPane* pane, int x, int y) {
    (void)pane;
    updateHoveredLibraryMousePosition(x, y);
}
