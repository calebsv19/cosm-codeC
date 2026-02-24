#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>


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
#include "ide/Panes/Terminal/input_terminal.h"
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
#include "core/Analysis/analysis_scheduler.h"
#include "core/LoopWake/mainthread_wake.h"
#include "core/LoopTimer/mainthread_timer_scheduler.h"
#include "core/LoopMessages/mainthread_message_queue.h"
#include "core/LoopJobs/mainthread_jobs.h"
#include "core/LoopKernel/mainthread_kernel.h"
#include "core/LoopTime/loop_time.h"
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
static bool s_loop_timers_registered = false;
static int s_file_watcher_timer_id = -1;
static int s_git_watcher_timer_id = -1;
static bool s_loop_diag_initialized = false;
static bool s_loop_diag_enabled = false;
static int s_loop_max_wait_ms_override = -1;

typedef struct LoopRuntimeDiag {
    Uint32 periodStartMs;
    Uint64 frames;
    Uint64 waitCalls;
    Uint64 blockedMs;
    Uint64 activeMs;
    Uint64 wakeDelta;
    Uint64 timerFiredDelta;
    uint32_t queueDepthPeak;
    uint32_t queueDepthLast;
    uint32_t lastWakeReceived;
    uint32_t lastTimerFired;
} LoopRuntimeDiag;

static LoopRuntimeDiag s_loop_diag = {0};

static void init_loop_diag_config(void) {
    if (s_loop_diag_initialized) return;
    const char* env = getenv("IDE_LOOP_DIAG_LOG");
    s_loop_diag_enabled = (env && env[0] &&
                           (strcmp(env, "1") == 0 || strcasecmp(env, "true") == 0));
    const char* waitEnv = getenv("IDE_LOOP_MAX_WAIT_MS");
    if (waitEnv && waitEnv[0]) {
        char* end = NULL;
        long v = strtol(waitEnv, &end, 10);
        if (end != waitEnv && v >= 1 && v <= 5000) {
            s_loop_max_wait_ms_override = (int)v;
        }
    }
    s_loop_diag_initialized = true;
}

static void loop_timer_file_watcher_cb(void* user_data) {
    (void)user_data;
    pollFileWatcher();
}

static void loop_timer_git_watcher_cb(void* user_data) {
    (void)user_data;
    pollGitStatusWatcher();
}

static void tick_command_bus_job(void* user_data) {
    (void)user_data;
    tickCommandBus();
}

static void ensure_loop_timers_registered(void) {
    if (s_loop_timers_registered) return;
    s_file_watcher_timer_id = mainthread_timer_schedule_repeating(fileWatcherPollIntervalMs(),
                                                                  loop_timer_file_watcher_cb,
                                                                  NULL,
                                                                  "file_watcher");
    s_git_watcher_timer_id = mainthread_timer_schedule_repeating(gitStatusWatchIntervalMs(),
                                                                 loop_timer_git_watcher_cb,
                                                                 NULL,
                                                                 "git_watcher");
    s_loop_timers_registered = true;
}

static void loop_diag_tick(uint64_t frameStartNs, uint64_t blockedNs, bool didWaitCall) {
    init_loop_diag_config();
    if (!s_loop_diag_enabled) return;

    uint64_t nowNs = loop_time_now_ns();
    Uint32 nowMs = (Uint32)(nowNs / 1000000ULL);
    if (s_loop_diag.periodStartMs == 0) {
        s_loop_diag.periodStartMs = nowMs;
        MainThreadWakeStats wake = {0};
        mainthread_wake_snapshot(&wake);
        s_loop_diag.lastWakeReceived = wake.received;
        MainThreadTimerSchedulerStats timerStats = {0};
        mainthread_timer_scheduler_snapshot(&timerStats);
        s_loop_diag.lastTimerFired = timerStats.fired_count;
    }

    uint64_t frameElapsedNs = loop_time_diff_ns(nowNs, frameStartNs);
    uint64_t activeNs = (frameElapsedNs > blockedNs) ? (frameElapsedNs - blockedNs) : 0;
    Uint32 blockedMs = (Uint32)(blockedNs / 1000000ULL);
    Uint32 activeMs = (Uint32)(activeNs / 1000000ULL);

    s_loop_diag.frames++;
    s_loop_diag.blockedMs += blockedMs;
    s_loop_diag.activeMs += activeMs;
    if (didWaitCall) s_loop_diag.waitCalls++;

    MainThreadWakeStats wake = {0};
    mainthread_wake_snapshot(&wake);
    if (wake.received >= s_loop_diag.lastWakeReceived) {
        s_loop_diag.wakeDelta += (wake.received - s_loop_diag.lastWakeReceived);
    }
    s_loop_diag.lastWakeReceived = wake.received;

    MainThreadTimerSchedulerStats timerStats = {0};
    mainthread_timer_scheduler_snapshot(&timerStats);
    if (timerStats.fired_count >= s_loop_diag.lastTimerFired) {
        s_loop_diag.timerFiredDelta += (timerStats.fired_count - s_loop_diag.lastTimerFired);
    }
    s_loop_diag.lastTimerFired = timerStats.fired_count;

    MainThreadMessageQueueStats msgStats = {0};
    mainthread_message_queue_snapshot(&msgStats);
    s_loop_diag.queueDepthLast = msgStats.depth;
    if (msgStats.depth > s_loop_diag.queueDepthPeak) {
        s_loop_diag.queueDepthPeak = msgStats.depth;
    }

    Uint32 periodMs = nowMs - s_loop_diag.periodStartMs;
    if (periodMs < 1000) return;

    Uint64 totalMs = s_loop_diag.blockedMs + s_loop_diag.activeMs;
    double blockedPct = (totalMs > 0) ? (100.0 * (double)s_loop_diag.blockedMs / (double)totalMs) : 0.0;
    double activePct = (totalMs > 0) ? (100.0 * (double)s_loop_diag.activeMs / (double)totalMs) : 0.0;
    printf("[LoopDiag] period=%ums frames=%llu waits=%llu blocked=%llums(%.1f%%) active=%llums(%.1f%%) wakes=%llu timers=%llu q_last=%u q_peak=%u\n",
           (unsigned int)periodMs,
           (unsigned long long)s_loop_diag.frames,
           (unsigned long long)s_loop_diag.waitCalls,
           (unsigned long long)s_loop_diag.blockedMs,
           blockedPct,
           (unsigned long long)s_loop_diag.activeMs,
           activePct,
           (unsigned long long)s_loop_diag.wakeDelta,
           (unsigned long long)s_loop_diag.timerFiredDelta,
           s_loop_diag.queueDepthLast,
           s_loop_diag.queueDepthPeak);

    s_loop_diag.periodStartMs = nowMs;
    s_loop_diag.frames = 0;
    s_loop_diag.waitCalls = 0;
    s_loop_diag.blockedMs = 0;
    s_loop_diag.activeMs = 0;
    s_loop_diag.wakeDelta = 0;
    s_loop_diag.timerFiredDelta = 0;
    s_loop_diag.queueDepthPeak = 0;
}

static void apply_worker_message(FrameContext* ctx, const MainThreadMessage* msg) {
    if (!ctx || !msg) return;

    if (msg->type == MAINTHREAD_MSG_ANALYSIS_FINISHED) {
        const MainThreadAnalysisFinishedPayload* p = &msg->payload.analysis_finished;
        if (p->project_root[0] && strcmp(p->project_root, projectPath) != 0) {
            return;
        }

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
        requestFullRedraw(RENDER_INVALIDATION_CONTENT | RENDER_INVALIDATION_BACKGROUND);
    }
}

static int drain_worker_messages(FrameContext* ctx) {
    enum { kDrainBudget = 64 };
    int applied = 0;
    MainThreadMessage msg;
    while (applied < kDrainBudget && mainthread_message_queue_pop(&msg)) {
        apply_worker_message(ctx, &msg);
        mainthread_message_release(&msg);
        applied++;
    }
    return applied;
}

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

static bool workspace_has_diagnostics_cache(const char* project_root) {
    if (!project_root || !project_root[0]) return false;
    char path[1024];
    snprintf(path, sizeof(path), "%s/ide_files/analysis_diagnostics.json", project_root);
    struct stat st;
    return (stat(path, &st) == 0) && S_ISREG(st.st_mode);
}

static bool update_layout_sync_state_snapshot(void) {
    RenderContext* rctx = getRenderContext();
    if (!rctx || !rctx->window) return true;

    int winW = 0, winH = 0;
    SDL_GetWindowSize(rctx->window, &winW, &winH);

    LayoutDimensions* dims = getLayoutDimensions();
    UIState* ui = getUIState();
    LayoutSyncState next;
    memset(&next, 0, sizeof(next));
    next.winW = winW;
    next.winH = winH;
    next.toolWidth = dims ? dims->toolWidth : 0;
    next.controlWidth = dims ? dims->controlWidth : 0;
    next.terminalHeight = dims ? dims->terminalHeight : 0;
    next.toolPanelVisible = ui ? ui->toolPanelVisible : false;
    next.controlPanelVisible = ui ? ui->controlPanelVisible : false;

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

static bool process_single_input_event(FrameContext* ctx,
                                       const SDL_Event* src,
                                       bool debugKeyLog) {
    if (!ctx || !src) return false;
    SDL_Event e = *src;
    if (mainthread_wake_is_event(&e)) {
        mainthread_wake_note_received();
        return false;
    }

    IDECoreState* core = getCoreState();
    UIPane* beforeMousePane = core ? core->activeMousePane : NULL;
    UIPane* beforeFocusedPane = core ? core->focusedPane : NULL;

    bool visualEvent = shouldInvalidateForEvent(&e);
    if (debugKeyLog && e.type == SDL_KEYDOWN) {
        SDL_Keycode key = e.key.keysym.sym;
        printf("KEYDOWN: %s (%d)\n", SDL_GetKeyName(key), key);
    }

    *ctx->event = e;
    handleInput(ctx->event, ctx->panes, *ctx->paneCount,
                ctx->resizeZones, *ctx->resizeZoneCount,
                ctx->paneCount, ctx->running);

    if (visualEvent) {
        invalidateInputTargetPanes(ctx, &e, beforeMousePane, beforeFocusedPane);
    }
    return visualEvent;
}

static bool processInputEvents(FrameContext* ctx, const SDL_Event* firstEvent) {
    bool sawVisualEvent = false;
    const char* debugKeysEnv = getenv("IDE_DEBUG_KEY_LOG");
    const bool debugKeyLog = (debugKeysEnv && debugKeysEnv[0] &&
                              (strcmp(debugKeysEnv, "1") == 0 ||
                               strcasecmp(debugKeysEnv, "true") == 0));

    bool havePendingMotion = false;
    SDL_Event pendingMotion;
    memset(&pendingMotion, 0, sizeof(pendingMotion));

    if (firstEvent) {
        if (firstEvent->type == SDL_MOUSEMOTION) {
            pendingMotion = *firstEvent;
            havePendingMotion = true;
        } else {
            sawVisualEvent |= process_single_input_event(ctx, firstEvent, debugKeyLog);
        }
    }

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_MOUSEMOTION) {
            pendingMotion = e;
            havePendingMotion = true;
            continue;
        }
        if (havePendingMotion) {
            sawVisualEvent |= process_single_input_event(ctx, &pendingMotion, debugKeyLog);
            havePendingMotion = false;
        }
        sawVisualEvent |= process_single_input_event(ctx, &e, debugKeyLog);
    }

    if (havePendingMotion) {
        sawVisualEvent |= process_single_input_event(ctx, &pendingMotion, debugKeyLog);
    }
    return sawVisualEvent;
}


static bool tickBackgroundSystems() {
    bool terminalChanged = false;
    ide_ipc_pump();
    mainthread_jobs_enqueue(tick_command_bus_job, NULL);
    mainthread_kernel_tick(loop_time_now_ns());
    terminalChanged = terminal_tick_backend();
    // tickDiagnosticsEngine(dt), tickUndoSystem(dt), etc.
    analysis_job_poll();
    return terminalChanged;
}

static bool main_loop_has_immediate_work(void) {
    if (pendingProjectRefresh) return true;
    if (hasFrameInvalidation()) return true;

    MainThreadMessageQueueStats msgStats = {0};
    mainthread_message_queue_snapshot(&msgStats);
    if (msgStats.depth > 0) return true;
    return false;
}

static int compute_wait_timeout_ms(FrameContext* ctx) {
    if (!ctx) return 0;

    IDECoreState* core = getCoreState();
    const bool activeInteraction =
        (core && core->projectDrag.active) || gResizeDrag.active || isRenaming();
    const bool invalidated = hasFrameInvalidation();

    const Uint64 perfNow = SDL_GetPerformanceCounter();
    const float freq = (float)SDL_GetPerformanceFrequency();
    const float elapsedSinceRender = (perfNow - *ctx->lastRender) / freq;

    int timeoutMs = -1;
    if (activeInteraction || invalidated) {
        float frameRemain = ctx->targetFrameTime - elapsedSinceRender;
        int frameMs = frameRemain > 0.0f ? (int)(frameRemain * 1000.0f) : 0;
        if (frameMs < 0) frameMs = 0;
        if (frameMs > 16) frameMs = 16;
        timeoutMs = frameMs;
    }

    Uint32 nextDeadlineMs = 0;
    if (mainthread_timer_scheduler_next_deadline_ms(&nextDeadlineMs)) {
        Uint32 nowMs = loop_time_now_ms32();
        int timerMs = (int)(nextDeadlineMs - nowMs);
        if (timerMs < 0) timerMs = 0;
        if (timeoutMs < 0 || timerMs < timeoutMs) timeoutMs = timerMs;
    }

    float heartbeat = getHeartbeatIntervalSeconds();
    int heartbeatMs = (int)((heartbeat - elapsedSinceRender) * 1000.0f);
    if (heartbeatMs < 0) heartbeatMs = 0;
    if (timeoutMs < 0 || heartbeatMs < timeoutMs) timeoutMs = heartbeatMs;

    if (timeoutMs < 0) timeoutMs = 500;
    if (!activeInteraction && timeoutMs > 500) timeoutMs = 500;
    init_loop_diag_config();
    if (s_loop_max_wait_ms_override > 0 && timeoutMs > s_loop_max_wait_ms_override) {
        timeoutMs = s_loop_max_wait_ms_override;
    }
    return timeoutMs;
}

static uint64_t wait_for_next_wake(FrameContext* ctx, bool* outWaitCalled) {
    if (outWaitCalled) *outWaitCalled = false;
    if (!ctx || !ctx->running || !(*ctx->running)) return 0;
    if (main_loop_has_immediate_work()) return 0;

    int timeoutMs = compute_wait_timeout_ms(ctx);
    if (timeoutMs < 0) return 0;

    if (outWaitCalled) *outWaitCalled = true;
    uint64_t waitStartNs = loop_time_now_ns();
    SDL_Event waitedEvent;
    if (mainthread_wake_wait_for_event((uint32_t)timeoutMs, &waitedEvent)) {
        processInputEvents(ctx, &waitedEvent);
    }
    uint64_t waitEndNs = loop_time_now_ns();
    return loop_time_diff_ns(waitEndNs, waitStartNs);
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
    uint64_t frameStartNs = loop_time_now_ns();
    uint64_t blockedNs = 0;
    bool didWaitCall = false;

    if (timerHudActive) ts_start_timer("SystemLoop");

    ensure_loop_timers_registered();

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
    processInputEvents(ctx, NULL);
    UIState* ui = getUIState();
    if (ui && ui->terminalVisible && ui->terminalPanel) {
        if (terminal_tick_drag_autoscroll(ui->terminalPanel, dt)) {
            invalidatePane(ui->terminalPanel,
                           RENDER_INVALIDATION_INPUT | RENDER_INVALIDATION_CONTENT);
            requestFullRedraw(RENDER_INVALIDATION_INPUT | RENDER_INVALIDATION_CONTENT);
        }
    }
    if (timerHudActive) ts_stop_timer("Input");

    if (forceFullRedrawEnabled()) {
        invalidateAll(ctx->panes, *ctx->paneCount,
                      RENDER_INVALIDATION_LAYOUT | RENDER_INVALIDATION_CONTENT);
        requestFullRedraw(RENDER_INVALIDATION_LAYOUT | RENDER_INVALIDATION_CONTENT);
    }

    if (timerHudActive) ts_start_timer("BackgroundTick");
    const bool terminalChanged = tickBackgroundSystems();
    drain_worker_messages(ctx);
    if (terminalChanged) {
        UIState* ui = getUIState();
        if (ui && ui->terminalPanel && ui->terminalVisible) {
            invalidatePane(ui->terminalPanel,
                           RENDER_INVALIDATION_CONTENT | RENDER_INVALIDATION_BACKGROUND);
        }
    }
    const WorkspaceBuildConfig* cfg = getWorkspaceBuildConfig();
    const char* buildArgs = (cfg && cfg->build_args[0]) ? cfg->build_args : NULL;
    analysis_scheduler_tick(projectPath, buildArgs);
    if (timerHudActive) ts_stop_timer("BackgroundTick");

    if (tickRenameAnimation(loop_time_now_ms32())) {
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
        unsigned int reason_mask = pendingProjectRefreshReasonMask;
        if (reason_mask == 0) {
            reason_mask = ANALYSIS_REASON_WORKSPACE_RELOAD;
        }
        bool force_full = false;
        bool slow_mode = false;
        if (reason_mask & ANALYSIS_REASON_WORKSPACE_RELOAD) {
            bool has_diag_cache = workspace_has_diagnostics_cache(projectPath);
            force_full = !has_diag_cache; // No cache => quick full rebuild.
            slow_mode = has_diag_cache;   // Cache exists => lazy background mode.
        }
        refreshProjectDirectory();
        analysis_status_set(ANALYSIS_STATUS_STALE_LOADING);
        analysis_job_set_slow_mode_next_run(slow_mode);
        analysis_scheduler_request((AnalysisRefreshReason)reason_mask, force_full);
        rebuildLibraryFlatRows();
        initAssetManagerPanel();
        resetGitTree();
        pendingProjectRefresh = false;
        pendingProjectRefreshReasonMask = 0;
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

    if (!didRender) {
        blockedNs = wait_for_next_wake(ctx, &didWaitCall);
    }
    loop_diag_tick(frameStartNs, blockedNs, didWaitCall);

    if (timerHudActive) ts_stop_timer("SystemLoop");
}
