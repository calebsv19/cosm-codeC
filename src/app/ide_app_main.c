#include "ide/ide_app_main.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <SDL2/SDL.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

//  GLOBAL HANDLING
#include "app/GlobalInfo/core_state.h"
#include "app/GlobalInfo/system_control.h"
#include "app/GlobalInfo/event_loop.h"
#include "app/GlobalInfo/runtime_startup_defaults.h"
#include "app/GlobalInfo/workspace_prefs.h"
#include "timer_hud/time_scope.h"

//  UI STATE
#include "ide/UI/layout.h"
#include "ide/UI/shared_theme_font_adapter.h"
#include "ide/Panes/Editor/editor_view.h"

int STARTING_WIDTH = 1600, STARTING_HEIGHT = 860;

typedef enum IdeAppStage {
    IDE_APP_STAGE_INIT = 0,
    IDE_APP_STAGE_BOOTSTRAPPED,
    IDE_APP_STAGE_CONFIG_LOADED,
    IDE_APP_STAGE_STATE_SEEDED,
    IDE_APP_STAGE_SUBSYSTEMS_READY,
    IDE_APP_STAGE_RUNTIME_STARTED,
    IDE_APP_STAGE_LOOP_COMPLETED,
    IDE_APP_STAGE_SHUTDOWN_COMPLETED
} IdeAppStage;

typedef struct IdeLaunchArgs {
    int argc;
    char **argv;
} IdeLaunchArgs;

typedef struct IdeDispatchRequest {
    int argc;
    char **argv;
    int (*legacy_entry)(int argc, char **argv);
} IdeDispatchRequest;

typedef struct IdeDispatchOutcome {
    bool dispatched;
    bool used_legacy_entry;
    int exit_code;
} IdeDispatchOutcome;

typedef struct IdeDispatchSummary {
    uint32_t dispatch_count;
    bool dispatch_succeeded;
    int last_dispatch_exit_code;
} IdeDispatchSummary;

typedef struct IdeLifecycleOwnership {
    bool bootstrap_owned;
    bool config_owned;
    bool state_seed_owned;
    bool subsystems_owned;
    bool runtime_owned;
    bool dispatch_owned;
    bool shutdown_owned;
} IdeLifecycleOwnership;

typedef struct IdeAppContext {
    IdeAppStage stage;
    int exit_code;
    int (*legacy_entry)(int argc, char **argv);
    bool (*runtime_dispatch)(const IdeDispatchRequest *request, IdeDispatchOutcome *outcome);
    IdeLaunchArgs launch_args;
    IdeDispatchSummary dispatch_summary;
    IdeLifecycleOwnership ownership;
    int wrapper_error;
} IdeAppContext;

typedef enum IdeWrapperError {
    IDE_WRAP_OK = 0,
    IDE_WRAP_BOOTSTRAP_FAILED = 1,
    IDE_WRAP_CONFIG_LOAD_FAILED = 2,
    IDE_WRAP_STATE_SEED_FAILED = 3,
    IDE_WRAP_SUBSYSTEMS_INIT_FAILED = 4,
    IDE_WRAP_RUNTIME_START_FAILED = 5,
    IDE_WRAP_DISPATCH_PREPARE_FAILED = 6,
    IDE_WRAP_DISPATCH_EXECUTE_FAILED = 7,
    IDE_WRAP_DISPATCH_FINALIZE_FAILED = 8,
    IDE_WRAP_RUN_LOOP_FAILED = 9
} IdeWrapperError;

static int ide_default_legacy_entry(int argc, char **argv) {
    (void)argc;
    (void)argv;
    return 1;
}

static bool ide_default_runtime_dispatch(const IdeDispatchRequest *request,
                                         IdeDispatchOutcome *outcome) {
    if (!request || !outcome || !request->legacy_entry) {
        return false;
    }
    outcome->dispatched = true;
    outcome->used_legacy_entry = true;
    outcome->exit_code = request->legacy_entry(request->argc, request->argv);
    return true;
}

static IdeAppContext g_ide_app_ctx = {
    .stage = IDE_APP_STAGE_INIT,
    .exit_code = 1,
    .legacy_entry = ide_default_legacy_entry,
    .runtime_dispatch = ide_default_runtime_dispatch
};

static void ide_log_stage_error(const char *fn_name,
                                const char *stage_name,
                                IdeAppStage expected,
                                IdeAppStage actual,
                                IdeAppStage next) {
    fprintf(stderr,
            "ide: lifecycle stage order violation fn=%s stage=%s (expected=%d actual=%d next=%d)\n",
            fn_name ? fn_name : "unknown",
            stage_name ? stage_name : "unknown",
            (int)expected,
            (int)actual,
            (int)next);
}

static void ide_log_wrapper_error(IdeWrapperError code,
                                  const char *fn_name,
                                  IdeAppStage stage,
                                  const char *reason) {
    fprintf(stderr,
            "ide: wrapper error code=%d fn=%s stage=%d reason=%s\n",
            (int)code,
            fn_name ? fn_name : "unknown",
            (int)stage,
            reason ? reason : "unspecified");
}

static bool ide_app_transition_stage(IdeAppContext *ctx, IdeAppStage expected, IdeAppStage next, const char *fn_name) {
    if (!ctx) {
        return false;
    }
    if (ctx->stage != expected) {
        ide_log_stage_error(fn_name, fn_name, expected, ctx->stage, next);
        return false;
    }
    ctx->stage = next;
    return true;
}

static bool ide_app_bootstrap_ctx(IdeAppContext *ctx) {
    int (*legacy_entry)(int argc, char **argv) = ide_default_legacy_entry;
    bool (*runtime_dispatch)(const IdeDispatchRequest *request, IdeDispatchOutcome *outcome) =
        ide_default_runtime_dispatch;
    IdeLaunchArgs launch_args = {0};
    if (!ctx) {
        return false;
    }
    if (ctx->legacy_entry) {
        legacy_entry = ctx->legacy_entry;
    }
    if (ctx->runtime_dispatch) {
        runtime_dispatch = ctx->runtime_dispatch;
    }
    launch_args = ctx->launch_args;

    memset(ctx, 0, sizeof(*ctx));
    ctx->legacy_entry = legacy_entry;
    ctx->runtime_dispatch = runtime_dispatch;
    ctx->launch_args = launch_args;
    ctx->exit_code = 1;
    ctx->dispatch_summary.last_dispatch_exit_code = 1;
    ctx->wrapper_error = IDE_WRAP_OK;

    if (!ide_app_transition_stage(ctx,
                                  IDE_APP_STAGE_INIT,
                                  IDE_APP_STAGE_BOOTSTRAPPED,
                                  "ide_app_bootstrap")) {
        return false;
    }
    ctx->ownership.bootstrap_owned = true;
    return true;
}

static bool ide_app_config_load_ctx(IdeAppContext *ctx) {
    if (!ctx) {
        return false;
    }
    if (!ide_app_transition_stage(ctx,
                                  IDE_APP_STAGE_BOOTSTRAPPED,
                                  IDE_APP_STAGE_CONFIG_LOADED,
                                  "ide_app_config_load")) {
        return false;
    }
    ctx->ownership.config_owned = true;
    return true;
}

static bool ide_app_state_seed_ctx(IdeAppContext *ctx) {
    if (!ctx) {
        return false;
    }
    if (!ide_app_transition_stage(ctx,
                                  IDE_APP_STAGE_CONFIG_LOADED,
                                  IDE_APP_STAGE_STATE_SEEDED,
                                  "ide_app_state_seed")) {
        return false;
    }
    ctx->ownership.state_seed_owned = true;
    return true;
}

static bool ide_app_subsystems_init_ctx(IdeAppContext *ctx) {
    if (!ctx) {
        return false;
    }
    if (!ide_app_transition_stage(ctx,
                                  IDE_APP_STAGE_STATE_SEEDED,
                                  IDE_APP_STAGE_SUBSYSTEMS_READY,
                                  "ide_app_subsystems_init")) {
        return false;
    }
    ctx->ownership.subsystems_owned = true;
    return true;
}

static bool ide_runtime_start_ctx(IdeAppContext *ctx) {
    if (!ctx) {
        return false;
    }
    if (!ide_app_transition_stage(ctx,
                                  IDE_APP_STAGE_SUBSYSTEMS_READY,
                                  IDE_APP_STAGE_RUNTIME_STARTED,
                                  "ide_runtime_start")) {
        return false;
    }
    ctx->ownership.runtime_owned = true;
    return true;
}

static bool ide_app_dispatch_prepare_ctx(IdeAppContext *ctx, IdeDispatchRequest *request) {
    if (!ctx || !request || !ctx->legacy_entry || !ctx->runtime_dispatch) {
        ide_log_wrapper_error(IDE_WRAP_DISPATCH_PREPARE_FAILED,
                              "ide_app_dispatch_prepare_ctx",
                              ctx ? ctx->stage : IDE_APP_STAGE_INIT,
                              "invalid wrapper context");
        return false;
    }
    if (ctx->stage != IDE_APP_STAGE_RUNTIME_STARTED) {
        ide_log_stage_error("ide_app_dispatch_prepare_ctx",
                            "runtime_started",
                            IDE_APP_STAGE_RUNTIME_STARTED,
                            ctx->stage,
                            ctx->stage);
        return false;
    }
    memset(request, 0, sizeof(*request));
    request->argc = ctx->launch_args.argc;
    request->argv = ctx->launch_args.argv;
    request->legacy_entry = ctx->legacy_entry;
    ctx->dispatch_summary.dispatch_count += 1u;
    ctx->ownership.dispatch_owned = true;
    return true;
}

static bool ide_app_dispatch_execute_ctx(IdeAppContext *ctx,
                                         const IdeDispatchRequest *request,
                                         IdeDispatchOutcome *outcome) {
    if (!ctx || !request || !outcome || !ctx->runtime_dispatch) {
        ide_log_wrapper_error(IDE_WRAP_DISPATCH_EXECUTE_FAILED,
                              "ide_app_dispatch_execute_ctx",
                              ctx ? ctx->stage : IDE_APP_STAGE_INIT,
                              "invalid wrapper context");
        return false;
    }
    memset(outcome, 0, sizeof(*outcome));
    return ctx->runtime_dispatch(request, outcome) && outcome->dispatched;
}

static int ide_app_dispatch_finalize_ctx(IdeAppContext *ctx, const IdeDispatchOutcome *outcome) {
    if (!ctx || !outcome) {
        if (ctx) {
            ctx->wrapper_error = IDE_WRAP_DISPATCH_FINALIZE_FAILED;
        }
        ide_log_wrapper_error(IDE_WRAP_DISPATCH_FINALIZE_FAILED,
                              "ide_app_dispatch_finalize_ctx",
                              ctx ? ctx->stage : IDE_APP_STAGE_INIT,
                              "invalid dispatch outcome");
        return IDE_WRAP_DISPATCH_FINALIZE_FAILED;
    }
    ctx->dispatch_summary.dispatch_succeeded = true;
    ctx->dispatch_summary.last_dispatch_exit_code = outcome->exit_code;
    ctx->exit_code = outcome->exit_code;
    if (!ide_app_transition_stage(ctx,
                                  IDE_APP_STAGE_RUNTIME_STARTED,
                                  IDE_APP_STAGE_LOOP_COMPLETED,
                                  "ide_app_dispatch_finalize_ctx")) {
        ctx->wrapper_error = IDE_WRAP_DISPATCH_FINALIZE_FAILED;
        ide_log_wrapper_error(IDE_WRAP_DISPATCH_FINALIZE_FAILED,
                              "ide_app_dispatch_finalize_ctx",
                              ctx->stage,
                              "stage transition failed");
        return IDE_WRAP_DISPATCH_FINALIZE_FAILED;
    }
    return ctx->exit_code;
}

static int ide_app_run_loop_ctx(IdeAppContext *ctx) {
    IdeDispatchRequest request = {0};
    IdeDispatchOutcome outcome = {0};
    if (!ide_app_dispatch_prepare_ctx(ctx, &request)) {
        if (ctx) {
            ctx->wrapper_error = IDE_WRAP_DISPATCH_PREPARE_FAILED;
        }
        return IDE_WRAP_DISPATCH_PREPARE_FAILED;
    }
    if (!ide_app_dispatch_execute_ctx(ctx, &request, &outcome)) {
        ctx->dispatch_summary.dispatch_succeeded = false;
        ctx->dispatch_summary.last_dispatch_exit_code = IDE_WRAP_DISPATCH_EXECUTE_FAILED;
        ctx->wrapper_error = IDE_WRAP_DISPATCH_EXECUTE_FAILED;
        ide_log_wrapper_error(IDE_WRAP_DISPATCH_EXECUTE_FAILED,
                              "ide_app_run_loop_ctx",
                              ctx->stage,
                              "dispatch execute failed");
        return IDE_WRAP_DISPATCH_EXECUTE_FAILED;
    }
    return ide_app_dispatch_finalize_ctx(ctx, &outcome);
}

static void ide_app_release_ownership_ctx(IdeAppContext *ctx) {
    if (!ctx) {
        return;
    }
    ctx->ownership.dispatch_owned = false;
    ctx->ownership.runtime_owned = false;
    ctx->ownership.subsystems_owned = false;
    ctx->ownership.state_seed_owned = false;
    ctx->ownership.config_owned = false;
    ctx->ownership.bootstrap_owned = false;
}

static void ide_app_shutdown_ctx(IdeAppContext *ctx) {
    if (!ctx) {
        return;
    }
    if (ctx->stage == IDE_APP_STAGE_SHUTDOWN_COMPLETED) {
        return;
    }
    ide_app_release_ownership_ctx(ctx);
    ctx->ownership.shutdown_owned = true;
    ctx->stage = IDE_APP_STAGE_SHUTDOWN_COMPLETED;
}

bool ide_app_bootstrap(void) {
    return ide_app_bootstrap_ctx(&g_ide_app_ctx);
}

bool ide_app_config_load(void) {
    return ide_app_config_load_ctx(&g_ide_app_ctx);
}

bool ide_app_state_seed(void) {
    return ide_app_state_seed_ctx(&g_ide_app_ctx);
}

bool ide_app_subsystems_init(void) {
    return ide_app_subsystems_init_ctx(&g_ide_app_ctx);
}

bool ide_runtime_start(void) {
    return ide_runtime_start_ctx(&g_ide_app_ctx);
}

void ide_app_set_legacy_entry(int (*legacy_entry)(int argc, char **argv)) {
    if (legacy_entry) {
        g_ide_app_ctx.legacy_entry = legacy_entry;
    }
}

int ide_app_run_loop(void) {
    return ide_app_run_loop_ctx(&g_ide_app_ctx);
}

void ide_app_shutdown(void) {
    ide_app_shutdown_ctx(&g_ide_app_ctx);
}

int ide_app_main(int argc, char **argv) {
    int exit_code = IDE_WRAP_BOOTSTRAP_FAILED;

    g_ide_app_ctx.launch_args.argc = argc;
    g_ide_app_ctx.launch_args.argv = argv;
    ide_app_set_legacy_entry(ide_app_main_legacy);

    if (!ide_app_bootstrap_ctx(&g_ide_app_ctx)) {
        g_ide_app_ctx.wrapper_error = IDE_WRAP_BOOTSTRAP_FAILED;
        ide_log_wrapper_error(IDE_WRAP_BOOTSTRAP_FAILED,
                              "ide_app_main",
                              g_ide_app_ctx.stage,
                              "bootstrap failed");
        return exit_code;
    }
    if (!ide_app_config_load_ctx(&g_ide_app_ctx)) {
        exit_code = IDE_WRAP_CONFIG_LOAD_FAILED;
        g_ide_app_ctx.wrapper_error = IDE_WRAP_CONFIG_LOAD_FAILED;
        ide_log_wrapper_error(IDE_WRAP_CONFIG_LOAD_FAILED,
                              "ide_app_main",
                              g_ide_app_ctx.stage,
                              "config load failed");
        goto shutdown;
    }
    if (!ide_app_state_seed_ctx(&g_ide_app_ctx)) {
        exit_code = IDE_WRAP_STATE_SEED_FAILED;
        g_ide_app_ctx.wrapper_error = IDE_WRAP_STATE_SEED_FAILED;
        ide_log_wrapper_error(IDE_WRAP_STATE_SEED_FAILED,
                              "ide_app_main",
                              g_ide_app_ctx.stage,
                              "state seed failed");
        goto shutdown;
    }
    if (!ide_app_subsystems_init_ctx(&g_ide_app_ctx)) {
        exit_code = IDE_WRAP_SUBSYSTEMS_INIT_FAILED;
        g_ide_app_ctx.wrapper_error = IDE_WRAP_SUBSYSTEMS_INIT_FAILED;
        ide_log_wrapper_error(IDE_WRAP_SUBSYSTEMS_INIT_FAILED,
                              "ide_app_main",
                              g_ide_app_ctx.stage,
                              "subsystems init failed");
        goto shutdown;
    }
    if (!ide_runtime_start_ctx(&g_ide_app_ctx)) {
        exit_code = IDE_WRAP_RUNTIME_START_FAILED;
        g_ide_app_ctx.wrapper_error = IDE_WRAP_RUNTIME_START_FAILED;
        ide_log_wrapper_error(IDE_WRAP_RUNTIME_START_FAILED,
                              "ide_app_main",
                              g_ide_app_ctx.stage,
                              "runtime start failed");
        goto shutdown;
    }

    exit_code = ide_app_run_loop_ctx(&g_ide_app_ctx);
    if (exit_code != 0 && g_ide_app_ctx.wrapper_error == IDE_WRAP_OK) {
        g_ide_app_ctx.wrapper_error = IDE_WRAP_RUN_LOOP_FAILED;
        ide_log_wrapper_error(IDE_WRAP_RUN_LOOP_FAILED,
                              "ide_app_main",
                              g_ide_app_ctx.stage,
                              "run loop failed");
    }
shutdown:
    ide_app_shutdown_ctx(&g_ide_app_ctx);
    fprintf(stderr,
            "ide: wrapper exit stage=%d exit_code=%d dispatch_count=%u dispatch_ok=%d last_dispatch_exit=%d wrapper_error=%d\n",
            (int)g_ide_app_ctx.stage,
            exit_code,
            (unsigned)g_ide_app_ctx.dispatch_summary.dispatch_count,
            g_ide_app_ctx.dispatch_summary.dispatch_succeeded ? 1 : 0,
            g_ide_app_ctx.dispatch_summary.last_dispatch_exit_code,
            g_ide_app_ctx.wrapper_error);
    return exit_code;
}

static const bool ENABLE_TIMER_HUD = false;

static bool parse_bool_token(const char *value, bool *out) {
    if (!value || !out) {
        return false;
    }
    if (strcmp(value, "1") == 0 || strcasecmp(value, "true") == 0 ||
        strcasecmp(value, "on") == 0 || strcasecmp(value, "yes") == 0) {
        *out = true;
        return true;
    }
    if (strcmp(value, "0") == 0 || strcasecmp(value, "false") == 0 ||
        strcasecmp(value, "off") == 0 || strcasecmp(value, "no") == 0) {
        *out = false;
        return true;
    }
    return false;
}

static bool resolve_timer_hud_enabled(int argc, char *argv[]) {
    bool enabled = ENABLE_TIMER_HUD;

    const char *env = getenv("IDE_TIMER_HUD");
    bool parsed_env = false;
    if (env) {
        parsed_env = parse_bool_token(env, &enabled);
        if (!parsed_env) {
            fprintf(stderr,
                    "[TimerHUD] Ignoring IDE_TIMER_HUD='%s' (expected 0/1/true/false/on/off).\n",
                    env);
        }
    }

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--timer-hud") == 0) {
            enabled = true;
        } else if (strcmp(argv[i], "--no-timer-hud") == 0) {
            enabled = false;
        }
    }

    return enabled;
}

int ide_app_main_legacy(int argc, char *argv[]) {
    setTimerHudEnabled(resolve_timer_hud_enabled(argc, argv));
    fprintf(stderr,
            "[TimerHUD] IDE profiling %s (enable with --timer-hud or IDE_TIMER_HUD=1)\n",
            isTimerHudEnabled() ? "ENABLED" : "DISABLED");
    ide_apply_runtime_startup_defaults();
    {
        const char *zoom_env = getenv("IDE_FONT_ZOOM_STEP");
        int persisted_zoom = 0;
        if ((!zoom_env || !zoom_env[0]) && loadFontZoomStepPreference(&persisted_zoom)) {
            char zoom_buf[16];
            snprintf(zoom_buf, sizeof(zoom_buf), "%d", persisted_zoom);
            setenv("IDE_FONT_ZOOM_STEP", zoom_buf, 1);
        }
    }

    if (!initializeSystem((argc > 0) ? argv[0] : NULL)) {
        return 1;
    }

    {
        const char *persisted_theme = loadThemePresetPreference();
        if (persisted_theme && persisted_theme[0]) {
            if (ide_shared_theme_set_preset(persisted_theme)) {
                ide_refresh_live_theme();
            }
        }
    }

    UIPane *panes[MAX_PANES];
    int pane_count = 0;

    ResizeZone resize_zones[5];
    int resize_zone_count = 0;

    int last_w = 0;
    int last_h = 0;

    bool running = true;
    SDL_Event event;

    Uint64 last_time = SDL_GetPerformanceCounter();
    Uint64 last_render = last_time;
    const float target_frame_time = 1.0f / 60.0f;

    IDECoreState *core = getCoreState();
    if (!core->persistentEditorView) {
        core->persistentEditorView = createEditorView();
    }
    if (!core->activeEditorView) {
        core->activeEditorView = (core->persistentEditorView->type == VIEW_LEAF)
            ? core->persistentEditorView
            : findNextLeaf(core->persistentEditorView);
        if (core->activeEditorView) {
            setActiveEditorView(core->activeEditorView);
        }
    }

    layout_static_panes(panes, &pane_count);

    FrameContext ctx = {
        .panes = panes,
        .paneCount = &pane_count,
        .resizeZones = resize_zones,
        .resizeZoneCount = &resize_zone_count,
        .running = &running,
        .event = &event,
        .lastW = &last_w,
        .lastH = &last_h,
        .targetFrameTime = target_frame_time,
        .lastRender = &last_render,
    };

    while (running) {
        Uint64 now = SDL_GetPerformanceCounter();
        float dt = (now - last_time) / (float)SDL_GetPerformanceFrequency();
        last_time = now;

        if (isTimerHudEnabled()) {
            ts_frame_start();
        }
        runFrameLoop(&ctx, now, dt);
        if (isTimerHudEnabled()) {
            ts_frame_end();
        }
    }

    shutdownSystem(panes, pane_count);
    return 0;
}
