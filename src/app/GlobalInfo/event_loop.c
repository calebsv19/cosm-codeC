#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>


#include "event_loop.h"
#include "system_control.h"
#include "core_state.h"

#include "ide/Panes/Editor/editor_view.h"
#include "ide/Panes/Editor/editor_state.h"

#include "core/InputManager/input_manager.h"
#include "core/CommandBus/command_bus.h"
#include "core/Watcher/file_watcher.h"
#include "core/Analysis/project_scan.h"
#include "core/Analysis/analysis_status.h"
#include "core/InputManager/UserInput/rename_flow.h"
#include "ide/Panes/Terminal/terminal.h"
#include "ide/Panes/Popup/popup_pane.h"
#include "ide/Panes/ToolPanels/Project/tool_project.h"
#include "ide/Panes/ToolPanels/Assets/tool_assets.h"
#include "ide/Panes/ToolPanels/Git/render_tool_git.h"
#include "ide/Panes/ToolPanels/Git/tool_git.h"
#include "ide/Panes/ToolPanels/Libraries/tool_libraries.h"

#include "ide/UI/layout.h"
#include "ide/UI/layout_config.h"
#include "ide/UI/ui_state.h"
#include "ide/UI/shared_theme_font_adapter.h"
#include "ide/UI/resize.h"
#include "engine/Render/render_pipeline.h"
#include "core/Analysis/library_index.h"
#include "core/Analysis/analysis_cache.h"
#include "core/Analysis/analysis_job.h"
#include "app/GlobalInfo/workspace_prefs.h"
#include "core/Ipc/ide_ipc_server.h"


// TimerHud extension
#include "timer_hud/time_scope.h"


//      ================================================
//                      Loop Logic


static EditorView* saveEditorViewState() {
    return (getCoreState()->editorPane) ? getCoreState()->editorPane->editorView : NULL;
}

typedef struct LayoutSyncState {
    int winW;
    int winH;
    int toolWidth;
    int controlWidth;
    int terminalHeight;
    bool toolPanelVisible;
    bool controlPanelVisible;
} LayoutSyncState;

static bool s_layout_sync_state_valid = false;
static LayoutSyncState s_layout_sync_state = {0};
static bool s_heartbeat_interval_initialized = false;
static float s_heartbeat_interval_seconds = 0.50f;
static bool s_force_full_redraw_initialized = false;
static bool s_force_full_redraw = false;

static float getHeartbeatIntervalSeconds(void) {
    if (!s_heartbeat_interval_initialized) {
        const char* env = getenv("IDE_RENDER_HEARTBEAT_MS");
        if (env && env[0]) {
            char* end = NULL;
            long ms = strtol(env, &end, 10);
            if (end != env && ms >= 50 && ms <= 2000) {
                s_heartbeat_interval_seconds = (float)ms / 1000.0f;
            }
        }
        s_heartbeat_interval_initialized = true;
    }
    return s_heartbeat_interval_seconds;
}

static bool forceFullRedrawEnabled(void) {
    if (!s_force_full_redraw_initialized) {
        const char* env = getenv("IDE_FORCE_FULL_REDRAW");
        s_force_full_redraw = (env && env[0] &&
                               (strcmp(env, "1") == 0 || strcasecmp(env, "true") == 0));
        s_force_full_redraw_initialized = true;
    }
    return s_force_full_redraw;
}

static bool update_layout_sync_state_snapshot(void) {
    RenderContext* rctx = getRenderContext();
    if (!rctx || !rctx->window) return true;

    int winW = 0, winH = 0;
    SDL_GetWindowSize(rctx->window, &winW, &winH);

    LayoutDimensions* dims = getLayoutDimensions();
    UIState* ui = getUIState();
    LayoutSyncState next = {
        .winW = winW,
        .winH = winH,
        .toolWidth = dims ? dims->toolWidth : 0,
        .controlWidth = dims ? dims->controlWidth : 0,
        .terminalHeight = dims ? dims->terminalHeight : 0,
        .toolPanelVisible = ui ? ui->toolPanelVisible : false,
        .controlPanelVisible = ui ? ui->controlPanelVisible : false,
    };

    if (!s_layout_sync_state_valid ||
        memcmp(&s_layout_sync_state, &next, sizeof(next)) != 0) {
        s_layout_sync_state = next;
        s_layout_sync_state_valid = true;
        return true;
    }
    return false;
}

static void layoutAndSyncPanes(UIPane** panes, int* paneCount) {
    layout_static_panes(panes, paneCount);
}

static bool shouldInvalidateForEvent(const SDL_Event* event) {
    if (!event) return false;

    switch (event->type) {
        case SDL_KEYDOWN:
        case SDL_KEYUP:
        case SDL_TEXTINPUT:
        case SDL_TEXTEDITING:
        case SDL_MOUSEMOTION:
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
        case SDL_MOUSEWHEEL:
        case SDL_DROPFILE:
        case SDL_DROPTEXT:
        case SDL_DROPBEGIN:
        case SDL_DROPCOMPLETE:
            return true;
        case SDL_WINDOWEVENT:
            switch (event->window.event) {
                case SDL_WINDOWEVENT_RESIZED:
                case SDL_WINDOWEVENT_SIZE_CHANGED:
                case SDL_WINDOWEVENT_EXPOSED:
                    return true;
                default:
                    return false;
            }
        default:
            return false;
    }
}

static void invalidateInputTargetPanes(FrameContext* ctx,
                                       const SDL_Event* event,
                                       UIPane* beforeMousePane,
                                       UIPane* beforeFocusedPane) {
    IDECoreState* core = getCoreState();
    if (!core || !event) return;

    UIPane* afterMousePane = core->activeMousePane;
    UIPane* afterFocusedPane = core->focusedPane;

    switch (event->type) {
        case SDL_KEYDOWN:
        case SDL_KEYUP:
        case SDL_TEXTINPUT:
        case SDL_TEXTEDITING:
            if (afterFocusedPane) {
                invalidatePane(afterFocusedPane,
                               RENDER_INVALIDATION_INPUT | RENDER_INVALIDATION_CONTENT);
            } else if (core->editorPane) {
                invalidatePane(core->editorPane,
                               RENDER_INVALIDATION_INPUT | RENDER_INVALIDATION_CONTENT);
            }
            break;
        case SDL_MOUSEMOTION:
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
        case SDL_MOUSEWHEEL:
            if (beforeMousePane) {
                invalidatePane(beforeMousePane, RENDER_INVALIDATION_INPUT);
            }
            if (afterMousePane && afterMousePane != beforeMousePane) {
                invalidatePane(afterMousePane, RENDER_INVALIDATION_INPUT);
            }
            if (!afterMousePane && core->editorPane) {
                invalidatePane(core->editorPane, RENDER_INVALIDATION_INPUT);
            }
            break;
        case SDL_WINDOWEVENT:
            if (event->window.event == SDL_WINDOWEVENT_RESIZED ||
                event->window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
                event->window.event == SDL_WINDOWEVENT_EXPOSED) {
                invalidateAll(ctx->panes, *ctx->paneCount,
                              RENDER_INVALIDATION_LAYOUT | RENDER_INVALIDATION_RESIZE);
                requestFullRedraw(RENDER_INVALIDATION_LAYOUT | RENDER_INVALIDATION_RESIZE);
            }
            break;
        default:
            if (beforeFocusedPane) {
                invalidatePane(beforeFocusedPane, RENDER_INVALIDATION_INPUT);
            }
            if (afterFocusedPane && afterFocusedPane != beforeFocusedPane) {
                invalidatePane(afterFocusedPane, RENDER_INVALIDATION_INPUT);
            }
            break;
    }
}

static bool processInputEvents(FrameContext* ctx) {
    bool sawVisualEvent = false;
    const char* debugKeysEnv = getenv("IDE_DEBUG_KEY_LOG");
    const bool debugKeyLog = (debugKeysEnv && debugKeysEnv[0] &&
                              (strcmp(debugKeysEnv, "1") == 0 ||
                               strcasecmp(debugKeysEnv, "true") == 0));
    while (SDL_PollEvent(ctx->event)) {
        IDECoreState* core = getCoreState();
        UIPane* beforeMousePane = core ? core->activeMousePane : NULL;
        UIPane* beforeFocusedPane = core ? core->focusedPane : NULL;
        if (shouldInvalidateForEvent(ctx->event)) {
            sawVisualEvent = true;
        }
        if (debugKeyLog && ctx->event->type == SDL_KEYDOWN) {
            SDL_Keycode key = ctx->event->key.keysym.sym;
            printf("KEYDOWN: %s (%d)\n", SDL_GetKeyName(key), key);
        }

        handleInput(ctx->event, ctx->panes, *ctx->paneCount,
                    ctx->resizeZones, *ctx->resizeZoneCount,
                    ctx->paneCount, ctx->running);

        if (shouldInvalidateForEvent(ctx->event)) {
            invalidateInputTargetPanes(ctx, ctx->event, beforeMousePane, beforeFocusedPane);
        }
    }
    return sawVisualEvent;
}


static bool tickBackgroundSystems() {
    bool terminalChanged = false;
    ide_ipc_pump();
    tickCommandBus();
    pollFileWatcher();
    terminalChanged = terminal_tick_backend();
    pollGitStatusWatcher();
    // tickDiagnosticsEngine(dt), tickUndoSystem(dt), etc.
    analysis_job_poll();
    return terminalChanged;
}

static void run_analysis_refresh_if_needed(void) {
    if (analysis_refresh_running() || !analysis_refresh_pending()) return;
    const WorkspaceBuildConfig* cfg = getWorkspaceBuildConfig();
    const char* buildArgs = (cfg && cfg->build_args[0]) ? cfg->build_args : NULL;
    start_async_workspace_analysis(projectPath, buildArgs);
}


//                      Loop Logic
//      ================================================
//                      Render Logic


static bool checkRenderFrame(FrameContext* ctx, Uint64 now) {
    float timeSinceLastRender = (now - *ctx->lastRender) / (float)SDL_GetPerformanceFrequency();
    const bool invalidated = hasFrameInvalidation();
    const float heartbeat = getHeartbeatIntervalSeconds();
    const bool invalidationDue = invalidated && (timeSinceLastRender >= ctx->targetFrameTime);
    const bool heartbeatDue = (!invalidated) && (timeSinceLastRender >= heartbeat);

    if (invalidationDue || heartbeatDue) {
        const bool timerHudActive = isTimerHudEnabled();
        RenderContext* rctx = getRenderContext();

        // Clear background color before drawing
        if (timerHudActive) ts_start_timer("ClearFrame");
        {
            SDL_Color bg = ide_shared_theme_background_color();
            SDL_SetRenderDrawColor(rctx->renderer, bg.r, bg.g, bg.b, bg.a);
        }
#if !USE_VULKAN
        SDL_RenderClear(rctx->renderer);
#endif
        if (timerHudActive) ts_stop_timer("ClearFrame");

        // Render all UI
        if (timerHudActive) ts_start_timer("RenderPipeline");
        RenderPipeline_renderAll(ctx->panes,
                                 *ctx->paneCount,
                                 ctx->lastW, ctx->lastH,
                                 ctx->resizeZones, ctx->resizeZoneCount, getCoreState());
        if (timerHudActive) ts_stop_timer("RenderPipeline");

        *ctx->lastRender = now;
        return true;
    }
    return false;
}


//                      Render Logic
//      ================================================
//                          Main


void runFrameLoop(FrameContext* ctx, Uint64 now, float dt) {
    IDECoreState* core = getCoreState();
    const bool timerHudActive = core->timerHudEnabled;
    static bool lastAnalysisRunning = false;

    if (timerHudActive) ts_start_timer("SystemLoop");

    EditorView* savedView = saveEditorViewState();

    if (timerHudActive) ts_start_timer("LayoutSync");
    if (update_layout_sync_state_snapshot()) {
        layoutAndSyncPanes(ctx->panes, ctx->paneCount);
        bindEditorViewToEditorPane(savedView, ctx->panes, *ctx->paneCount);
        invalidateAll(ctx->panes, *ctx->paneCount,
                      RENDER_INVALIDATION_LAYOUT | RENDER_INVALIDATION_RESIZE);
        requestFullRedraw(RENDER_INVALIDATION_LAYOUT | RENDER_INVALIDATION_RESIZE);
    }
    if (timerHudActive) ts_stop_timer("LayoutSync");

    if (timerHudActive) ts_start_timer("Input");
    processInputEvents(ctx);
    if (timerHudActive) ts_stop_timer("Input");

    if (forceFullRedrawEnabled()) {
        invalidateAll(ctx->panes, *ctx->paneCount,
                      RENDER_INVALIDATION_LAYOUT | RENDER_INVALIDATION_CONTENT);
        requestFullRedraw(RENDER_INVALIDATION_LAYOUT | RENDER_INVALIDATION_CONTENT);
    }

    if (timerHudActive) ts_start_timer("BackgroundTick");
    const bool terminalChanged = tickBackgroundSystems();
    if (terminalChanged) {
        UIState* ui = getUIState();
        if (ui && ui->terminalPanel && ui->terminalVisible) {
            invalidatePane(ui->terminalPanel,
                           RENDER_INVALIDATION_CONTENT | RENDER_INVALIDATION_BACKGROUND);
        }
    }
    run_analysis_refresh_if_needed();
    bool analysisRunning = analysis_refresh_running();
    if (lastAnalysisRunning && !analysisRunning) {
        rebuildLibraryFlatRows();
        UIState* ui = getUIState();
        if (ui) {
            invalidatePane(ui->toolPanel,
                           RENDER_INVALIDATION_CONTENT | RENDER_INVALIDATION_BACKGROUND);
            invalidatePane(ui->controlPanel,
                           RENDER_INVALIDATION_CONTENT | RENDER_INVALIDATION_BACKGROUND);
            invalidatePane(ui->editorPanel,
                           RENDER_INVALIDATION_CONTENT | RENDER_INVALIDATION_BACKGROUND);
        } else {
            invalidateAll(ctx->panes, *ctx->paneCount,
                          RENDER_INVALIDATION_CONTENT | RENDER_INVALIDATION_BACKGROUND);
        }
    }
    lastAnalysisRunning = analysisRunning;
    if (timerHudActive) ts_stop_timer("BackgroundTick");

    if (tickRenameAnimation(SDL_GetTicks())) {
        requestFullRedraw(RENDER_INVALIDATION_OVERLAY);
    }

    if (core->projectDrag.active) {
        requestFullRedraw(RENDER_INVALIDATION_OVERLAY);
    }

    if (timerHudActive) ts_start_timer("RenderGate");
    bool didRender = checkRenderFrame(ctx, now);
    if (timerHudActive) ts_stop_timer("RenderGate");

    if (pendingProjectRefresh) {
        if (timerHudActive) ts_start_timer("ProjectRefresh");
        refreshProjectDirectory();
        analysis_status_set(ANALYSIS_STATUS_STALE_LOADING);
        analysis_request_refresh();
        rebuildLibraryFlatRows();
        initAssetManagerPanel();
        resetGitTree();
        pendingProjectRefresh = false;
        invalidateAll(ctx->panes, *ctx->paneCount,
                      RENDER_INVALIDATION_CONTENT | RENDER_INVALIDATION_BACKGROUND);
        requestFullRedraw(RENDER_INVALIDATION_CONTENT | RENDER_INVALIDATION_LAYOUT);
        if (timerHudActive) ts_stop_timer("ProjectRefresh");
    }

    if (didRender) {
        uint64_t renderedFrameId = 0;
        if (consumeFrameInvalidation(NULL, NULL, &renderedFrameId)) {
            for (int i = 0; i < *ctx->paneCount; ++i) {
                UIPane* pane = ctx->panes[i];
                if (!pane) continue;
                pane->lastRenderFrameId = renderedFrameId;
                paneClearDirty(pane);
            }
        }
    }

    // Soft frame pacing: yield briefly when we're early for next render frame.
    if (!didRender) {
        float elapsedSinceRender = (now - *ctx->lastRender) / (float)SDL_GetPerformanceFrequency();
        float remaining = hasFrameInvalidation()
            ? (ctx->targetFrameTime - elapsedSinceRender)
            : (getHeartbeatIntervalSeconds() - elapsedSinceRender);
        if (remaining > 0.002f) {
            Uint32 delayMs = (Uint32)(remaining * 1000.0f) - 1u;
            Uint32 maxDelay = hasFrameInvalidation() ? 4u : 16u;
            if (delayMs > maxDelay) delayMs = maxDelay;
            if (delayMs >= 1u) SDL_Delay(delayMs);
        }
    }

    if (timerHudActive) ts_stop_timer("SystemLoop");
}
