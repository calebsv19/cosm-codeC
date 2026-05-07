#include "engine/Render/timer_hud_adapter.h"

#include "timer_hud/time_scope.h"
#include "timer_hud/settings_loader.h"
#include "timer_hud/timer_hud_backend.h"
#include "engine/Render/render_pipeline.h"
#include "engine/Render/render_font.h"
#include "engine/Render/render_helpers.h"
#include "engine/Render/render_text_helpers.h"
#include "build_config.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#if USE_VULKAN
#include "vk_renderer_sdl.h"
#endif

static SDL_Renderer* g_timer_hud_renderer = NULL;
static TimerHUDSession* g_timer_hud_session = NULL;
static int g_logged_missing_renderer = 0;
static int g_logged_missing_font = 0;

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#if IDE_TIMER_HUD_LEGACY_SESSION_COMPAT
struct TimerHUDSession {
    unsigned char initialized;
};

static TimerHUDSession g_timer_hud_legacy_session = {0};

static const char* timer_hud_visual_mode_name(TimerHUDVisualMode mode) {
    switch (mode) {
        case TIMER_HUD_VISUAL_MODE_HISTORY_GRAPH: return "history_graph";
        case TIMER_HUD_VISUAL_MODE_HYBRID: return "hybrid";
        case TIMER_HUD_VISUAL_MODE_TEXT_COMPACT:
        default: return "text_compact";
    }
}

TimerHUDSession* ts_session_create(void) {
    return &g_timer_hud_legacy_session;
}

void ts_session_destroy(TimerHUDSession* session) {
    if (session && session->initialized) {
        ts_shutdown();
        session->initialized = 0u;
    }
}

void ts_session_register_backend(TimerHUDSession* session, const TimerHUDBackend* backend) {
    (void)session;
    ts_register_backend(backend);
}

bool ts_session_apply_init_config(TimerHUDSession* session, const TimerHUDInitConfig* config) {
    (void)session;
    if (!config) return false;
    ts_set_program_name(config->program_name);
    ts_set_output_root(config->output_root);
    ts_set_settings_path(config->settings_path);
    if (config->seed_settings_if_missing && config->default_settings_path) {
        (void)config->default_settings_path;
    }
    return true;
}

void ts_session_init(TimerHUDSession* session) {
    if (!session || session->initialized) return;
    ts_init();
    session->initialized = 1u;
}

void ts_session_shutdown(TimerHUDSession* session) {
    if (!session || !session->initialized) return;
    ts_shutdown();
    session->initialized = 0u;
}

void ts_session_start_timer(TimerHUDSession* session, const char* name) {
    (void)session;
    ts_start_timer(name);
}

void ts_session_stop_timer(TimerHUDSession* session, const char* name) {
    (void)session;
    ts_stop_timer(name);
}

void ts_session_frame_start(TimerHUDSession* session) {
    (void)session;
    ts_frame_start();
}

void ts_session_frame_end(TimerHUDSession* session) {
    (void)session;
    ts_frame_end();
}

void ts_session_render(TimerHUDSession* session) {
    (void)session;
    ts_render();
}

void ts_session_set_hud_enabled(TimerHUDSession* session, bool enabled) {
    (void)session;
    ts_settings.hud_enabled = enabled;
}

bool ts_session_is_hud_enabled(const TimerHUDSession* session) {
    (void)session;
    return ts_settings.hud_enabled;
}

bool ts_session_is_log_enabled(const TimerHUDSession* session) {
    (void)session;
    return ts_settings.log_enabled;
}

bool ts_session_is_event_tagging_enabled(const TimerHUDSession* session) {
    (void)session;
    return ts_settings.event_tagging_enabled;
}

TimerHUDVisualMode ts_visual_mode_from_string(const char* mode) {
    if (!mode) return TIMER_HUD_VISUAL_MODE_TEXT_COMPACT;
    if (strcmp(mode, "history_graph") == 0) return TIMER_HUD_VISUAL_MODE_HISTORY_GRAPH;
    if (strcmp(mode, "hybrid") == 0) return TIMER_HUD_VISUAL_MODE_HYBRID;
    return TIMER_HUD_VISUAL_MODE_TEXT_COMPACT;
}

bool ts_session_set_hud_visual_mode_kind(TimerHUDSession* session, TimerHUDVisualMode mode) {
    (void)session;
    ts_set_hud_visual_mode(timer_hud_visual_mode_name(mode));
    return true;
}

const char* ts_session_get_hud_visual_mode(const TimerHUDSession* session) {
    (void)session;
    return ts_settings.hud_visual_mode;
}

const char* ts_session_get_log_filepath(const TimerHUDSession* session) {
    (void)session;
    return ts_settings.log_filepath;
}
#endif

TimerHUDSession* timer_hud_session(void) {
    if (!g_timer_hud_session) {
        g_timer_hud_session = ts_session_create();
    }
    return g_timer_hud_session;
}

static TTF_Font* timer_hud_resolve_font(void) {
    TTF_Font* font = getUIFontByTier(CORE_FONT_TEXT_SIZE_BASIC);
    if (font) return font;
    return getActiveFont();
}

static int timer_hud_get_screen_size(int* out_w, int* out_h) {
    RenderContext* ctx = getRenderContext();
    if (!ctx || !g_timer_hud_renderer) {
        if (!g_logged_missing_renderer) {
            fprintf(stderr, "[TimerHUD] renderer not bound; HUD draw skipped.\n");
            g_logged_missing_renderer = 1;
        }
        return 0;
    }
    if (out_w) *out_w = ctx->width;
    if (out_h) *out_h = ctx->height;
    return 1;
}

static int timer_hud_measure_text(const char* text, int* out_w, int* out_h) {
    TTF_Font* font = timer_hud_resolve_font();
    if (!text) return 0;
    if (!font) {
        if (out_w) *out_w = (int)strlen(text) * 8;
        if (out_h) *out_h = 14;
        if (!g_logged_missing_font) {
            fprintf(stderr, "[TimerHUD] no font available yet; using estimated text bounds.\n");
            g_logged_missing_font = 1;
        }
        return 1;
    }
    if (out_w) *out_w = getTextWidthUTF8WithFont(text, font);
    if (out_h) *out_h = TTF_FontHeight(font);
    return 1;
}

static int timer_hud_line_height(void) {
    TTF_Font* font = timer_hud_resolve_font();
    if (!font) return 14;
    return TTF_FontHeight(font);
}

static void timer_hud_draw_rect(int x, int y, int w, int h, TimerHUDColor color) {
    if (!g_timer_hud_renderer) return;
    SDL_Rect rect = {x, y, w, h};
#if !USE_VULKAN
    SDL_SetRenderDrawBlendMode(g_timer_hud_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_timer_hud_renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(g_timer_hud_renderer, &rect);
#else
    vk_renderer_set_draw_color((VkRenderer*)g_timer_hud_renderer,
                               color.r / 255.0f,
                               color.g / 255.0f,
                               color.b / 255.0f,
                               color.a / 255.0f);
    vk_renderer_fill_rect((VkRenderer*)g_timer_hud_renderer, &rect);
#endif
}

static void timer_hud_draw_line(int x1, int y1, int x2, int y2, TimerHUDColor color) {
    if (!g_timer_hud_renderer) return;
#if !USE_VULKAN
    SDL_SetRenderDrawBlendMode(g_timer_hud_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_timer_hud_renderer, color.r, color.g, color.b, color.a);
    SDL_RenderDrawLine(g_timer_hud_renderer, x1, y1, x2, y2);
#else
    vk_renderer_set_draw_color((VkRenderer*)g_timer_hud_renderer,
                               color.r / 255.0f,
                               color.g / 255.0f,
                               color.b / 255.0f,
                               color.a / 255.0f);
    SDL_RenderDrawLine(g_timer_hud_renderer, x1, y1, x2, y2);
#endif
}

static void timer_hud_draw_text(const char* text, int x, int y, int align_flags, TimerHUDColor color) {
    if (!g_timer_hud_renderer) return;
    TTF_Font* font = timer_hud_resolve_font();
    if (!text || !font) return;
    SDL_Rect dst = {
        x,
        y,
        getTextWidthUTF8WithFont(text, font),
        TTF_FontHeight(font)
    };

    if (align_flags & TIMER_HUD_ALIGN_CENTER)  dst.x -= dst.w / 2;
    if (align_flags & TIMER_HUD_ALIGN_RIGHT)   dst.x -= dst.w;
    if (align_flags & TIMER_HUD_ALIGN_MIDDLE)  dst.y -= dst.h / 2;
    if (align_flags & TIMER_HUD_ALIGN_BOTTOM)  dst.y -= dst.h;

    drawTextUTF8WithFontColor(dst.x, dst.y, text, font,
                              (SDL_Color){color.r, color.g, color.b, color.a}, false);
}

static const TimerHUDBackend g_timer_hud_backend = {
    .init = NULL,
    .shutdown = NULL,
    .get_screen_size = timer_hud_get_screen_size,
    .measure_text = timer_hud_measure_text,
    .get_line_height = timer_hud_line_height,
    .draw_rect = timer_hud_draw_rect,
    .draw_line = timer_hud_draw_line,
    .draw_text = timer_hud_draw_text,
    .hud_padding = 6,
    .hud_spacing = 4,
    .hud_bg_alpha = 180
};

static int timer_hud_build_home_root(char* out_path, size_t out_cap) {
    const char* home = getenv("HOME");
    if (!out_path || out_cap == 0 || !home || !home[0]) {
        return 0;
    }
    return snprintf(out_path, out_cap, "%s/.custom_c_ide", home) < (int)out_cap;
}

static int timer_hud_build_home_settings_path(char* out_path, size_t out_cap) {
    const char* home = getenv("HOME");
    if (!out_path || out_cap == 0 || !home || !home[0]) {
        return 0;
    }
    return snprintf(out_path, out_cap, "%s/.custom_c_ide/timerhud/ide/settings.json", home) < (int)out_cap;
}

static int timer_hud_build_legacy_settings_path(char* out_path, size_t out_cap) {
    char cwd[PATH_MAX];
    if (!out_path || out_cap == 0 || !getcwd(cwd, sizeof(cwd))) {
        return 0;
    }
    return snprintf(out_path, out_cap, "%s/timerhud/ide/settings.json", cwd) < (int)out_cap;
}

void timer_hud_register_backend(void) {
    TimerHUDSession* session = timer_hud_session();
    char home_root[PATH_MAX];
    char home_settings_path[PATH_MAX];
    char legacy_settings_path[PATH_MAX];
    TimerHUDInitConfig init_config = {
        .program_name = "ide",
        .output_root = NULL,
        .settings_path = NULL,
        .default_settings_path = NULL,
        .seed_settings_if_missing = false,
    };
    if (!session) {
        fprintf(stderr, "[TimerHUD] failed to allocate IDE session.\n");
        return;
    }
    ts_session_register_backend(session, &g_timer_hud_backend);

    if (timer_hud_build_home_root(home_root, sizeof(home_root))) {
        init_config.output_root = home_root;
    }
    if (timer_hud_build_home_settings_path(home_settings_path, sizeof(home_settings_path))) {
        init_config.settings_path = home_settings_path;
        if (timer_hud_build_legacy_settings_path(legacy_settings_path, sizeof(legacy_settings_path)) &&
            access(home_settings_path, F_OK) != 0 &&
            access(legacy_settings_path, F_OK) == 0) {
            init_config.default_settings_path = legacy_settings_path;
            init_config.seed_settings_if_missing = true;
        }
    }

    const char* outputRoot = getenv("TIMERHUD_OUTPUT_ROOT");
    if (outputRoot && outputRoot[0]) {
        init_config.output_root = outputRoot;
    }

    const char* overridePath = getenv("IDE_TIMER_HUD_SETTINGS");
    if (overridePath && overridePath[0]) {
        init_config.settings_path = overridePath;
        fprintf(stderr, "[TimerHUD] settings path override: %s\n", overridePath);
    }

    (void)ts_session_apply_init_config(session, &init_config);
}

bool timer_hud_session_supports_runtime_work(void) {
    TimerHUDSession* session = timer_hud_session();
    if (!session) {
        return false;
    }
    return ts_session_is_hud_enabled(session) ||
           ts_session_is_log_enabled(session) ||
           ts_session_is_event_tagging_enabled(session);
}

void timer_hud_shutdown_session(void) {
    if (!g_timer_hud_session) {
        return;
    }
    ts_session_destroy(g_timer_hud_session);
    g_timer_hud_session = NULL;
}

void timer_hud_bind_renderer(void* renderer) {
    g_timer_hud_renderer = (SDL_Renderer*)renderer;
    if (g_timer_hud_renderer) {
        g_logged_missing_renderer = 0;
    }
}
