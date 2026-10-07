void ide_panel_text_cancel_pointer_drafts(void) {}
void cancelRenameComposition(void) {}
#include "ide/UI/window_lifecycle.h"
#include "ide/UI/layout_config.h"
#include "core/InputManager/input_resize.h"
#include "app/GlobalInfo/core_state.h"
#include "ide/Panes/Editor/editor_view.h"
#include "engine/Render/render_pipeline.h"
#include <assert.h>
void ide_button_scopes_cancel_except(UIPane* pane) { (void)pane; }
#include <stdio.h>
static LayoutDimensions layout = {200, 220, 180};
static IDECoreState core;
static EditorViewState editor_state;
static RenderContext render_context;
static int menu_cancels, mouse_cancels, redraws, layouts;
ResizeDragState gResizeDrag;
IDECoreState* getCoreState(void) { return &core; }
LayoutDimensions* getLayoutDimensions(void) { return &layout; }
RenderContext* getRenderContext(void) { return &render_context; }
void ide_menu_bar_cancel(void) { ++menu_cancels; }
void cancelMouseInputCapture(void) { ++mouse_cancels; core.activeMousePane = NULL; }
void resetProjectDragState(void) { core.projectDrag.entry = NULL; }
void requestFullRedraw(uint32_t reasons) { (void)reasons; ++redraws; }
void layout_static_panes(UIPane** panes, int* count) { (void)panes; (void)count; ++layouts; }
int main(void) {
    core.editorViewState = &editor_state;
    UIPane pane = {0}; UIPane* panes[] = {&pane}; int count = 1;
    const Uint8 events[] = {SDL_WINDOWEVENT_RESIZED, SDL_WINDOWEVENT_SIZE_CHANGED, SDL_WINDOWEVENT_MOVED,
        SDL_WINDOWEVENT_DISPLAY_CHANGED, SDL_WINDOWEVENT_MINIMIZED, SDL_WINDOWEVENT_MAXIMIZED,
        SDL_WINDOWEVENT_RESTORED, SDL_WINDOWEVENT_SHOWN, SDL_WINDOWEVENT_HIDDEN, SDL_WINDOWEVENT_FOCUS_LOST,
        SDL_WINDOWEVENT_LEAVE};
    EditorView split = {.splitRatio = 0.8f};
    for (unsigned i = 0; i < sizeof(events); ++i) {
        layout = (LayoutDimensions){400, 450, 500};
        gResizeDrag = (ResizeDragState){.active=true, .target=RESIZE_CORNER_TOOL, .startValueX=200, .startValueY=180};
        editor_state.draggingSplitDivider = true; editor_state.draggingSplit = &split;
        editor_state.splitDragStartRatio = 0.5f; split.splitRatio = 0.8f;
        editor_state.draggingScrollbar = true;
        SDL_Event e = {0}; e.type = SDL_WINDOWEVENT; e.window.event = events[i];
        ide_window_lifecycle_event(&e, panes, &count);
        assert(!gResizeDrag.active && layout.toolWidth==200 && layout.terminalHeight==180);
        assert(layout.controlWidth==450 && split.splitRatio==0.5f);
        assert(!editor_state.draggingSplitDivider && !editor_state.draggingScrollbar);
    }
    assert(menu_cancels == 11 && mouse_cancels == 11 && redraws == 11 && layouts == 11);
    SDL_Event e = {0}; e.type=SDL_KEYDOWN;
    ide_window_lifecycle_event(&e, panes, &count); assert(menu_cancels==11);
    for (int target=RESIZE_TOOL;target<=RESIZE_CORNER_CONTROL;++target) {
        layout=(LayoutDimensions){200,220,180};
        ResizeZone zone={{10,10,10,10},target};
        e.type=SDL_MOUSEBUTTONDOWN;e.button.button=SDL_BUTTON_LEFT;e.button.x=15;e.button.y=15;
        handleResizeDragging(&e,&zone,1,panes,&count);assert(gResizeDrag.active);
        e.type=SDL_MOUSEMOTION;e.motion.x=40;e.motion.y=40;
        handleResizeDragging(&e,&zone,1,panes,&count);
        cancelResizeDragging(panes,&count);
        assert(layout.toolWidth==200 && layout.controlWidth==220 && layout.terminalHeight==180);
        e.type=SDL_MOUSEBUTTONUP;e.button.button=SDL_BUTTON_LEFT;
        handleResizeDragging(&e,&zone,1,panes,&count);assert(!gResizeDrag.active);
    }
    puts("all invalidating window events restore splitter drafts and cancel capture; ordinary input preserved");
    return 0;
}

void ide_panel_fields_blur_except(UIPane* pane) { (void)pane; }
