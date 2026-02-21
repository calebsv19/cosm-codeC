#include "engine/Render/timer_hud_adapter.h"

#include "timer_hud/time_scope.h"
#include "timer_hud/timer_hud_backend.h"
#include "engine/Render/render_pipeline.h"
#include "engine/Render/render_font.h"
#include "build_config.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#if USE_VULKAN
#include "vk_renderer_sdl.h"
#endif

static SDL_Renderer* g_timer_hud_renderer = NULL;
static int g_logged_missing_renderer = 0;
static int g_logged_missing_font = 0;

static TTF_Font* timer_hud_resolve_font(void) {
    TTF_Font* font = getActiveFont();
    if (font) return font;

    static TTF_Font* fallbackFont = NULL;
    if (fallbackFont) return fallbackFont;

    const char* candidates[] = {
        "include/fonts/Montserrat/Montserrat-Medium.ttf",
        "include/fonts/Lato/Lato-Regular.ttf",
        "/System/Library/Fonts/Menlo.ttc"
    };

    for (int i = 0; i < (int)(sizeof(candidates) / sizeof(candidates[0])); ++i) {
        fallbackFont = TTF_OpenFont(candidates[i], 14);
        if (fallbackFont) return fallbackFont;
    }
    return NULL;
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
    if (TTF_SizeUTF8(font, text, out_w, out_h) != 0) {
        if (out_w) *out_w = (int)strlen(text) * 8;
        if (out_h) *out_h = 14;
        return 1;
    }
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

static void timer_hud_draw_text(const char* text, int x, int y, int align_flags, TimerHUDColor color) {
    if (!g_timer_hud_renderer) return;
    TTF_Font* font = timer_hud_resolve_font();
    if (!text || !font) return;

    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text,
                                                  (SDL_Color){color.r, color.g, color.b, color.a});
    if (!surface) return;

    SDL_Rect dst = {x, y, surface->w, surface->h};

    if (align_flags & TIMER_HUD_ALIGN_CENTER)  dst.x -= surface->w / 2;
    if (align_flags & TIMER_HUD_ALIGN_RIGHT)   dst.x -= surface->w;
    if (align_flags & TIMER_HUD_ALIGN_MIDDLE)  dst.y -= surface->h / 2;
    if (align_flags & TIMER_HUD_ALIGN_BOTTOM)  dst.y -= surface->h;

#if USE_VULKAN
    VkRendererTexture texture = {0};
    VkResult uploadResult = vk_renderer_upload_sdl_surface_with_filter(
        (VkRenderer*)g_timer_hud_renderer, surface, &texture, VK_FILTER_NEAREST);
    if (uploadResult == VK_SUCCESS) {
        vk_renderer_draw_texture((VkRenderer*)g_timer_hud_renderer, &texture, NULL, &dst);
        vk_renderer_queue_texture_destroy((VkRenderer*)g_timer_hud_renderer, &texture);
    }
#else
    SDL_Texture* texture = SDL_CreateTextureFromSurface(g_timer_hud_renderer, surface);
    if (texture) {
        SDL_RenderCopy(g_timer_hud_renderer, texture, NULL, &dst);
        SDL_DestroyTexture(texture);
    }
#endif

    SDL_FreeSurface(surface);
}

static const TimerHUDBackend g_timer_hud_backend = {
    .init = NULL,
    .shutdown = NULL,
    .get_screen_size = timer_hud_get_screen_size,
    .measure_text = timer_hud_measure_text,
    .get_line_height = timer_hud_line_height,
    .draw_rect = timer_hud_draw_rect,
    .draw_text = timer_hud_draw_text,
    .hud_padding = 6,
    .hud_spacing = 4,
    .hud_bg_alpha = 180
};

void timer_hud_register_backend(void) {
    ts_register_backend(&g_timer_hud_backend);
    ts_set_program_name("ide");

    const char* outputRoot = getenv("TIMERHUD_OUTPUT_ROOT");
    if (outputRoot && outputRoot[0]) {
        ts_set_output_root(outputRoot);
    }

    const char* overridePath = getenv("IDE_TIMER_HUD_SETTINGS");
    if (overridePath && overridePath[0]) {
        ts_set_settings_path(overridePath);
        fprintf(stderr, "[TimerHUD] settings path override: %s\n", overridePath);
    }
}

void timer_hud_bind_renderer(void* renderer) {
    g_timer_hud_renderer = (SDL_Renderer*)renderer;
    if (g_timer_hud_renderer) {
        g_logged_missing_renderer = 0;
    }
}
