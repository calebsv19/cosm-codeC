#include <SDL2/SDL.h>
#include <stdio.h>


#include "event_loop.h"
#include "system_control.h"
#include "core_state.h"

#include "ide/Panes/Editor/editor_view.h"
#include "ide/Panes/Editor/editor_state.h"

#include "core/InputManager/input_manager.h"
#include "core/CommandBus/command_bus.h"
#include "core/Watcher/file_watcher.h"
#include "core/Analysis/project_scan.h"
#include "ide/Panes/Terminal/terminal.h"
#include "ide/Panes/Popup/popup_pane.h"
#include "ide/Panes/ToolPanels/Project/tool_project.h"
#include "ide/Panes/ToolPanels/Assets/tool_assets.h"
#include "ide/Panes/ToolPanels/Git/render_tool_git.h"
#include "ide/Panes/ToolPanels/Libraries/tool_libraries.h"

#include "ide/UI/layout.h"
#include "ide/UI/resize.h"
#include "engine/Render/render_pipeline.h"
#include "core/Analysis/library_index.h"


// TimerHud extension
#include "engine/TimerHUD/src/api/time_scope.h"


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
    terminal_tick_backend();
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
#if !USE_VULKAN
        SDL_RenderClear(rctx->renderer);
#endif

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
    IDECoreState* core = getCoreState();
    const bool timerHudActive = core->timerHudEnabled;

    if (timerHudActive) ts_start_timer("SystemLoop");

    EditorView* savedView = saveEditorViewState();

    if (timerHudActive) ts_start_timer("Other");
    layoutAndSyncPanes(ctx->panes, ctx->paneCount);
    bindEditorViewToEditorPane(savedView, ctx->panes, *ctx->paneCount);
    if (timerHudActive) ts_stop_timer("Other");    

    if (timerHudActive) ts_start_timer("Input");
    processInputEvents(ctx);
    if (timerHudActive) ts_stop_timer("Input");

    if (timerHudActive) ts_start_timer("BackgroundTick");
    tickBackgroundSystems();
    if (timerHudActive) ts_stop_timer("BackgroundTick");

    
    checkRenderFrame(ctx, now);

    if (pendingProjectRefresh) {
        refreshProjectDirectory();
        analysis_scan_workspace(projectPath);
        library_index_build_workspace(projectPath);
        rebuildLibraryFlatRows();
        initAssetManagerPanel();
        resetGitTree();
        pendingProjectRefresh = false;
    }

    if (timerHudActive) ts_stop_timer("SystemLoop");
}
