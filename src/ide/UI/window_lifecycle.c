#include "window_lifecycle.h"
#include "panel_field_focus.h"
#include "panel_button_scope.h"
#include "core/InputManager/UserInput/rename_flow.h"
#include "button_scope.h"
#include "core/InputManager/input_mouse.h"
#include "core/InputManager/input_resize.h"
#include "ide/Panes/MenuBar/input_menu_bar.h"
#include "ide/Panes/Editor/editor_view.h"
#include "ide/Panes/ToolPanels/Project/tool_project.h"
#include "ide/UI/layout.h"
#include "engine/Render/render_pipeline.h"
#include "app/GlobalInfo/core_state.h"

static KitUiWindowState observation;
static SDL_Window* observed_window;

void ide_window_cancel_pointer_drafts(UIPane** panes, int* pane_count) {
    ide_panel_text_cancel_pointer_drafts();
    ide_menu_bar_cancel();
    ide_button_scopes_cancel_except(NULL);
    cancelMouseInputCapture();
    resetProjectDragState();
    cancelResizeDragging(panes, pane_count);
    IDECoreState* core = getCoreState();
    EditorViewState* state = core ? core->editorViewState : NULL;
    if (state) {
        if (state->draggingSplitDivider && state->draggingSplit) {
            state->draggingSplit->splitRatio = state->splitDragStartRatio;
        }
        state->draggingSplitDivider = false;
        state->draggingSplit = NULL;
        state->draggingScrollbar = false;
        state->draggingPane = NULL;
        state->draggingView = NULL;
    }
    requestFullRedraw(RENDER_INVALIDATION_LAYOUT | RENDER_INVALIDATION_INPUT);
}

void ide_window_lifecycle_event(const SDL_Event* event, UIPane** panes, int* pane_count) {
    if (!event) return;
    if ((event->type == SDL_WINDOWEVENT && event->window.event == SDL_WINDOWEVENT_FOCUS_LOST) ||
        event->type == SDL_APP_WILLENTERBACKGROUND || event->type == SDL_QUIT) {
        ide_panel_fields_blur_except(NULL);
        cancelRenameComposition();
    }
    if (kit_ui_window_event_invalidates_sdl(event) ||
        (event->type == SDL_WINDOWEVENT && event->window.event == SDL_WINDOWEVENT_LEAVE) ||
        event->type == SDL_APP_WILLENTERBACKGROUND || event->type == SDL_QUIT ||
        event->type == SDL_APP_TERMINATING) {
        ide_window_cancel_pointer_drafts(panes, pane_count);
    }
}

bool ide_window_lifecycle_refresh(UIPane** panes, int* pane_count) {
    RenderContext* ctx = getRenderContext();
    if (!ctx || !ctx->window) return false;
    if (observed_window != ctx->window) {
        observation = (KitUiWindowState){0};
        observed_window = ctx->window;
    }
    int initialized = observation.initialized;
    uint32_t changes = 0;
    CoreResult result = kit_ui_window_refresh_sdl(&observation, ctx->window, &changes);
    if (result.code != CORE_OK) return false;
    if (initialized && changes) ide_window_cancel_pointer_drafts(panes, pane_count);
    return observation.presentable != 0;
}
