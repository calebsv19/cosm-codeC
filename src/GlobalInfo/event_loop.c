#include <SDL2/SDL.h>
#include <stdio.h>


#include "event_loop.h"
#include "system_control.h"
#include "core_state.h"

#include "Editor/editor_view.h"
#include "Editor/editor_state.h"

#include "InputManager/input_manager.h"
#include "CommandBus/command_bus.h"
#include "Watcher/file_watcher.h"
#include "Popup/popup_pane.h"


#include "UI/layout.h"
#include "UI/resize.h"
#include "Render/render_pipeline.h"




//      ================================================
//                      Loop Logic


static EditorView* saveEditorViewState() {
    return (getCoreState()->editorPane) ? getCoreState()->editorPane->editorView : NULL;
}

static void layoutAndSyncPanes(UIPane** panes, int* paneCount) {
    layout_static_panes(panes, paneCount);
    syncPopupPane(panes, paneCount);
}

static void processInputEvents(FrameContext* ctx) {
    RenderContext* rctx = getRenderContext();

    while (SDL_PollEvent(ctx->event)) {
        if (ctx->event->type == SDL_KEYDOWN) {
            SDL_Keycode key = ctx->event->key.keysym.sym;
            printf("KEYDOWN: %s (%d)\n", SDL_GetKeyName(key), key);
        }

        handleInput(ctx->event, ctx->panes, *ctx->paneCount,
                    ctx->resizeZones, *ctx->resizeZoneCount,
                    ctx->paneCount, ctx->running);
    }
}


static void tickBackgroundSystems() {
    tickCommandBus();
    pollFileWatcher();
    // tickDiagnosticsEngine(dt), tickUndoSystem(dt), etc.
}


//                      Loop Logic
//      ================================================
//                      Render Logic


static void checkRenderFrame(FrameContext* ctx, Uint64 now) {
    float timeSinceLastRender = (now - *ctx->lastRender) / (float)SDL_GetPerformanceFrequency();

    if (timeSinceLastRender >= ctx->targetFrameTime) {
        RenderContext* rctx = getRenderContext();

        // Clear background color before drawing
        SDL_SetRenderDrawColor(rctx->renderer, 30, 30, 30, 255);
        SDL_RenderClear(rctx->renderer);

        // Render all UI
        RenderPipeline_renderAll(ctx->panes,
                                 *ctx->paneCount,
                                 ctx->lastW, ctx->lastH,
                                 ctx->resizeZones, ctx->resizeZoneCount, getCoreState());

        *ctx->lastRender = now;
    }
}


//                      Render Logic
//      ================================================
//                          Main


void runFrameLoop(FrameContext* ctx, Uint64 now, float dt) {
    EditorView* savedView = saveEditorViewState();

    layoutAndSyncPanes(ctx->panes, ctx->paneCount);
    bindEditorViewToEditorPane(savedView, ctx->panes, *ctx->paneCount);
    processInputEvents(ctx);
    tickBackgroundSystems();
    checkRenderFrame(ctx, now);
}

