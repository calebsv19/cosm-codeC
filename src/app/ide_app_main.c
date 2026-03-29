#include "ide/ide_app_main.h"

#include <stdbool.h>
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

typedef struct IdeLifecycleState {
    bool bootstrapped;
    bool config_loaded;
    bool state_seeded;
    bool subsystems_initialized;
    bool runtime_started;
    bool run_loop_completed;
    bool shutdown_completed;
    int exit_code;
} IdeLifecycleState;

static IdeLifecycleState g_ide_lifecycle = {0};

static int g_ide_launch_argc = 0;
static char **g_ide_launch_argv = NULL;

static int ide_default_legacy_entry(int argc, char **argv) {
    (void)argc;
    (void)argv;
    return 1;
}

static int (*g_ide_legacy_entry)(int argc, char **argv) = ide_default_legacy_entry;

bool ide_app_bootstrap(void) {
    memset(&g_ide_lifecycle, 0, sizeof(g_ide_lifecycle));
    g_ide_lifecycle.bootstrapped = true;
    return true;
}

bool ide_app_config_load(void) {
    if (!g_ide_lifecycle.bootstrapped) {
        return false;
    }
    g_ide_lifecycle.config_loaded = true;
    return true;
}

bool ide_app_state_seed(void) {
    if (!g_ide_lifecycle.config_loaded) {
        return false;
    }
    g_ide_lifecycle.state_seeded = true;
    return true;
}

bool ide_app_subsystems_init(void) {
    if (!g_ide_lifecycle.state_seeded) {
        return false;
    }
    g_ide_lifecycle.subsystems_initialized = true;
    return true;
}

bool ide_runtime_start(void) {
    if (!g_ide_lifecycle.subsystems_initialized) {
        return false;
    }
    g_ide_lifecycle.runtime_started = true;
    return true;
}

void ide_app_set_legacy_entry(int (*legacy_entry)(int argc, char **argv)) {
    if (legacy_entry) {
        g_ide_legacy_entry = legacy_entry;
    }
}

int ide_app_run_loop(void) {
    if (!g_ide_lifecycle.runtime_started) {
        return 1;
    }
    g_ide_lifecycle.exit_code = g_ide_legacy_entry(g_ide_launch_argc, g_ide_launch_argv);
    g_ide_lifecycle.run_loop_completed = true;
    return g_ide_lifecycle.exit_code;
}

void ide_app_shutdown(void) {
    if (!g_ide_lifecycle.bootstrapped) {
        return;
    }
    g_ide_lifecycle.shutdown_completed = true;
}

int ide_app_main(int argc, char **argv) {
    int exit_code = 1;

    g_ide_launch_argc = argc;
    g_ide_launch_argv = argv;
    ide_app_set_legacy_entry(ide_app_main_legacy);

    if (!ide_app_bootstrap()) {
        return exit_code;
    }
    if (!ide_app_config_load()) {
        ide_app_shutdown();
        return exit_code;
    }
    if (!ide_app_state_seed()) {
        ide_app_shutdown();
        return exit_code;
    }
    if (!ide_app_subsystems_init()) {
        ide_app_shutdown();
        return exit_code;
    }
    if (!ide_runtime_start()) {
        ide_app_shutdown();
        return exit_code;
    }

    exit_code = ide_app_run_loop();
    ide_app_shutdown();
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
