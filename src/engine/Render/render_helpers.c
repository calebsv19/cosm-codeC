#include "engine/Render/render_helpers.h"
#include "engine/Render/render_text_helpers.h"
#include "engine/Render/render_pipeline.h"  // for getRenderContext
#include "engine/Render/render_font.h"      // for getActiveFont
#include "ide/UI/shared_theme_font_adapter.h"
#include "core/TextSelection/text_selection_manager.h"
#include "ide/Panes/PaneInfo/pane.h"

#include <SDL2/SDL_ttf.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#define CLIP_STACK_MAX 16
static SDL_Rect s_clip_stack[CLIP_STACK_MAX];
static bool s_clip_enabled_stack[CLIP_STACK_MAX];
static int s_clip_top = 0;

#define TEXT_CACHE_BUCKETS 192
#define TEXT_CACHE_WAYS 4
#define TEXT_CACHE_MAX_TEXT_LEN 512
#define TEXT_CACHE_MAX_BYTES (32u * 1024u * 1024u)

typedef struct TextCacheEntry {
    bool valid;
    uint64_t last_used;
    SDL_Renderer* renderer;
    TTF_Font* font;
    uint32_t color_rgba;
    bool utf8;
    size_t text_len;
    uint32_t text_hash;
    char* text;
    int width;
    int height;
    size_t bytes_estimate;
#if USE_VULKAN
    VkRendererTexture texture;
#else
    SDL_Texture* texture;
#endif
} TextCacheEntry;

static TextCacheEntry s_text_cache[TEXT_CACHE_BUCKETS * TEXT_CACHE_WAYS];
static uint64_t s_text_cache_tick = 1;
static size_t s_text_cache_bytes = 0;

static uint32_t text_cache_hash_bytes(const char* text, size_t len) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < len; ++i) {
        h ^= (uint8_t)text[i];
        h *= 16777619u;
    }
    return h;
}

static uint32_t text_cache_pack_color(SDL_Color color) {
    return ((uint32_t)color.r << 24) | ((uint32_t)color.g << 16) |
           ((uint32_t)color.b << 8) | (uint32_t)color.a;
}

static uint32_t text_cache_bucket(SDL_Renderer* renderer,
                                  TTF_Font* font,
                                  uint32_t color_rgba,
                                  bool utf8,
                                  uint32_t text_hash) {
    uint64_t k = (uint64_t)(uintptr_t)renderer;
    k ^= ((uint64_t)(uintptr_t)font << 1);
    k ^= ((uint64_t)color_rgba << 32);
    k ^= (uint64_t)text_hash;
    k ^= utf8 ? 0x9e3779b97f4a7c15ull : 0x85ebca6b;
    return (uint32_t)(k % TEXT_CACHE_BUCKETS);
}

static void text_cache_destroy_entry(TextCacheEntry* e) {
    if (!e || !e->valid) return;
    if (s_text_cache_bytes >= e->bytes_estimate) {
        s_text_cache_bytes -= e->bytes_estimate;
    } else {
        s_text_cache_bytes = 0;
    }
#if USE_VULKAN
    if (e->renderer) {
        vk_renderer_queue_texture_destroy((VkRenderer*)e->renderer, &e->texture);
    }
#else
    if (e->texture) {
        SDL_DestroyTexture(e->texture);
    }
#endif
    free(e->text);
    memset(e, 0, sizeof(*e));
}

static TextCacheEntry* text_cache_find_global_lru(void) {
    TextCacheEntry* oldest = NULL;
    size_t count = sizeof(s_text_cache) / sizeof(s_text_cache[0]);
    for (size_t i = 0; i < count; ++i) {
        TextCacheEntry* e = &s_text_cache[i];
        if (!e->valid) continue;
        if (!oldest || e->last_used < oldest->last_used) {
            oldest = e;
        }
    }
    return oldest;
}

static bool text_cache_ensure_space(size_t needed_bytes) {
    if (needed_bytes > TEXT_CACHE_MAX_BYTES) return false;
    while (s_text_cache_bytes + needed_bytes > TEXT_CACHE_MAX_BYTES) {
        TextCacheEntry* victim = text_cache_find_global_lru();
        if (!victim) break;
        text_cache_destroy_entry(victim);
    }
    return (s_text_cache_bytes + needed_bytes <= TEXT_CACHE_MAX_BYTES);
}

static TextCacheEntry* text_cache_find_or_slot(SDL_Renderer* renderer,
                                               TTF_Font* font,
                                               const char* text,
                                               size_t text_len,
                                               uint32_t color_rgba,
                                               bool utf8,
                                               uint32_t text_hash,
                                               bool* out_hit) {
    *out_hit = false;
    uint32_t b = text_cache_bucket(renderer, font, color_rgba, utf8, text_hash);
    TextCacheEntry* oldest = NULL;
    for (uint32_t w = 0; w < TEXT_CACHE_WAYS; ++w) {
        TextCacheEntry* e = &s_text_cache[b * TEXT_CACHE_WAYS + w];
        if (!e->valid) {
            return e;
        }
        if (e->renderer == renderer &&
            e->font == font &&
            e->color_rgba == color_rgba &&
            e->utf8 == utf8 &&
            e->text_len == text_len &&
            e->text_hash == text_hash &&
            e->text &&
            memcmp(e->text, text, text_len) == 0) {
            *out_hit = true;
            return e;
        }
        if (!oldest || e->last_used < oldest->last_used) {
            oldest = e;
        }
    }
    return oldest;
}

static bool text_cache_get(SDL_Renderer* renderer,
                           TTF_Font* font,
                           const char* text,
                           SDL_Color color,
                           bool utf8,
                           TextCacheEntry** out_entry) {
    if (!renderer || !font || !text || text[0] == '\0') return false;
    size_t text_len = strlen(text);
    if (text_len == 0 || text_len > TEXT_CACHE_MAX_TEXT_LEN) return false;

    uint32_t color_rgba = text_cache_pack_color(color);
    uint32_t text_hash = text_cache_hash_bytes(text, text_len);
    bool hit = false;
    TextCacheEntry* slot = text_cache_find_or_slot(
        renderer, font, text, text_len, color_rgba, utf8, text_hash, &hit);
    if (!slot) return false;

    if (hit) {
        slot->last_used = s_text_cache_tick++;
        *out_entry = slot;
        return true;
    }

    SDL_Surface* surface =
        utf8 ? TTF_RenderUTF8_Blended(font, text, color)
             : TTF_RenderText_Blended(font, text, color);
    if (!surface) return false;

    text_cache_destroy_entry(slot);
    slot->renderer = renderer;
    slot->font = font;
    slot->color_rgba = color_rgba;
    slot->utf8 = utf8;
    slot->text_len = text_len;
    slot->text_hash = text_hash;
    slot->last_used = s_text_cache_tick++;
    slot->width = surface->w;
    slot->height = surface->h;
    slot->bytes_estimate = (size_t)surface->w * (size_t)surface->h * 4u;
    if (!text_cache_ensure_space(slot->bytes_estimate)) {
        memset(slot, 0, sizeof(*slot));
        SDL_FreeSurface(surface);
        return false;
    }
    slot->text = (char*)malloc(text_len + 1);
    if (!slot->text) {
        memset(slot, 0, sizeof(*slot));
        SDL_FreeSurface(surface);
        return false;
    }
    memcpy(slot->text, text, text_len + 1);

#if USE_VULKAN
    VkRendererTexture tex = {0};
    VkResult up = vk_renderer_upload_sdl_surface_with_filter(
        renderer, surface, &tex, VK_FILTER_LINEAR);
    SDL_FreeSurface(surface);
    if (up != VK_SUCCESS) {
        text_cache_destroy_entry(slot);
        return false;
    }
    slot->texture = tex;
#else
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    if (!tex) {
        text_cache_destroy_entry(slot);
        return false;
    }
    slot->texture = tex;
#endif
    slot->valid = true;
    s_text_cache_bytes += slot->bytes_estimate;
    *out_entry = slot;
    return true;
}

static void text_cache_draw_entry(SDL_Renderer* renderer,
                                  const TextCacheEntry* entry,
                                  int x,
                                  int y,
                                  bool bold) {
    if (!renderer || !entry || !entry->valid) return;
    SDL_Rect dst = {x, y, entry->width, entry->height};
#if USE_VULKAN
    SDL_Rect src = {0, 0, entry->width, entry->height};
    vk_renderer_draw_texture(renderer, &entry->texture, &src, &dst);
    if (bold) {
        dst.x += 1;
        vk_renderer_draw_texture(renderer, &entry->texture, &src, &dst);
    }
#else
    SDL_RenderCopy(renderer, entry->texture, NULL, &dst);
    if (bold) {
        dst.x += 1;
        SDL_RenderCopy(renderer, entry->texture, NULL, &dst);
    }
#endif
}

static void text_cache_draw_entry_clipped(SDL_Renderer* renderer,
                                          const TextCacheEntry* entry,
                                          int x,
                                          int y,
                                          bool bold,
                                          const SDL_Rect* clipRect) {
    if (!renderer || !entry || !entry->valid) return;
    if (!clipRect || clipRect->w <= 0 || clipRect->h <= 0) {
        text_cache_draw_entry(renderer, entry, x, y, bold);
        return;
    }

    SDL_Rect dst = {x, y, entry->width, entry->height};
    SDL_Rect vis = {0, 0, 0, 0};
    if (!SDL_IntersectRect(&dst, clipRect, &vis)) return;

    SDL_Rect src = {
        vis.x - dst.x,
        vis.y - dst.y,
        vis.w,
        vis.h
    };

#if USE_VULKAN
    vk_renderer_draw_texture(renderer, &entry->texture, &src, &vis);
    if (bold) {
        SDL_Rect dst2 = { x + 1, y, entry->width, entry->height };
        SDL_Rect vis2 = {0, 0, 0, 0};
        if (SDL_IntersectRect(&dst2, clipRect, &vis2)) {
            SDL_Rect src2 = {
                vis2.x - dst2.x,
                vis2.y - dst2.y,
                vis2.w,
                vis2.h
            };
            vk_renderer_draw_texture(renderer, &entry->texture, &src2, &vis2);
        }
    }
#else
    SDL_RenderCopy(renderer, entry->texture, &src, &vis);
    if (bold) {
        SDL_Rect dst2 = { x + 1, y, entry->width, entry->height };
        SDL_Rect vis2 = {0, 0, 0, 0};
        if (SDL_IntersectRect(&dst2, clipRect, &vis2)) {
            SDL_Rect src2 = {
                vis2.x - dst2.x,
                vis2.y - dst2.y,
                vis2.w,
                vis2.h
            };
            SDL_RenderCopy(renderer, entry->texture, &src2, &vis2);
        }
    }
#endif
}

void drawText(int x, int y, const char* text) {
    drawTextWithFont(x, y, text, getActiveFont());
}

void drawTextWithTier(int x, int y, const char* text, CoreFontTextSizeTier tier) {
    drawTextWithFont(x, y, text, getUIFontByTier(tier));
}

void drawTextWithFont(int x, int y, const char* text, TTF_Font* font) {
    if (!text || text[0] == '\0') return;
    if (!font) return;

    RenderContext* ctx = getRenderContext();
    if (!ctx || !ctx->renderer) return;

    SDL_Renderer* renderer = ctx->renderer;

    SDL_Color color = {255, 255, 255, 255};
    TextCacheEntry* cached = NULL;
    if (text_cache_get(renderer, font, text, color, false, &cached)) {
        text_cache_draw_entry(renderer, cached, x, y, false);
        return;
    }

#if USE_VULKAN
    SDL_Surface* surface = TTF_RenderText_Blended(font, text, color);
    if (!surface) return;
    VkRendererTexture vkTexture = {0};
    SDL_Rect srcRect = {0, 0, surface->w, surface->h};
    SDL_Rect dst = {x, y, surface->w, surface->h};
    if (vk_renderer_upload_sdl_surface_with_filter(
            renderer, surface, &vkTexture, VK_FILTER_LINEAR) == VK_SUCCESS) {
        vk_renderer_draw_texture(renderer, &vkTexture, &srcRect, &dst);
        vk_renderer_queue_texture_destroy(renderer, &vkTexture);
    }
    SDL_FreeSurface(surface);
#else
    SDL_Surface* surface = TTF_RenderText_Blended(font, text, color);
    if (!surface) return;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture) {
        SDL_Rect dst = {x, y, surface->w, surface->h};
        SDL_RenderCopy(renderer, texture, NULL, &dst);
        SDL_DestroyTexture(texture);
    }
    SDL_FreeSurface(surface);
#endif
}

void drawTextUTF8WithFontColor(int x, int y, const char* text, TTF_Font* font,
                               SDL_Color color, bool bold) {
    if (!text || text[0] == '\0') return;
    if (!font) return;

    RenderContext* ctx = getRenderContext();
    if (!ctx || !ctx->renderer) return;

    SDL_Renderer* renderer = ctx->renderer;
    TextCacheEntry* cached = NULL;
    if (text_cache_get(renderer, font, text, color, true, &cached)) {
        text_cache_draw_entry(renderer, cached, x, y, bold);
        return;
    }

#if USE_VULKAN
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text, color);
    if (!surface) return;
    VkRendererTexture vkTexture = {0};
    SDL_Rect srcRect = {0, 0, surface->w, surface->h};
    SDL_Rect dst = {x, y, surface->w, surface->h};
    if (vk_renderer_upload_sdl_surface_with_filter(renderer, surface, &vkTexture,
                                                   VK_FILTER_LINEAR) == VK_SUCCESS) {
        vk_renderer_draw_texture(renderer, &vkTexture, &srcRect, &dst);
        if (bold) {
            SDL_Rect dst2 = dst;
            dst2.x += 1;
            vk_renderer_draw_texture(renderer, &vkTexture, &srcRect, &dst2);
        }
        vk_renderer_queue_texture_destroy(renderer, &vkTexture);
    }
    SDL_FreeSurface(surface);
#else
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text, color);
    if (!surface) return;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture) {
        SDL_Rect dst = {x, y, surface->w, surface->h};
        SDL_RenderCopy(renderer, texture, NULL, &dst);
        if (bold) {
            SDL_Rect dst2 = dst;
            dst2.x += 1;
            SDL_RenderCopy(renderer, texture, NULL, &dst2);
        }
        SDL_DestroyTexture(texture);
    }
    SDL_FreeSurface(surface);
#endif
}

void drawTextUTF8WithFontColorClipped(int x, int y, const char* text, TTF_Font* font,
                                      SDL_Color color, bool bold, const SDL_Rect* clipRect) {
    if (!text || text[0] == '\0') return;
    if (!font) return;

    RenderContext* ctx = getRenderContext();
    if (!ctx || !ctx->renderer) return;

    SDL_Renderer* renderer = ctx->renderer;
    TextCacheEntry* cached = NULL;
    if (text_cache_get(renderer, font, text, color, true, &cached)) {
        text_cache_draw_entry_clipped(renderer, cached, x, y, bold, clipRect);
    }
}

void drawClippedText(int x, int y, const char* text, int maxWidth) {
    if (!text || text[0] == '\0' || maxWidth <= 0) return;

    TTF_Font* font = getActiveFont();
    size_t cutoff = getTextClampedLengthWithFont(text, maxWidth, font);
    if (cutoff == 0) return;

    char temp[1024] = {0};
    size_t copyLen = cutoff;
    if (copyLen >= sizeof(temp)) {
        copyLen = sizeof(temp) - 1;
    }
    strncpy(temp, text, copyLen);
    temp[copyLen] = '\0';

    drawTextWithFont(x, y, temp, font);
}


void renderButton(UIPane* pane, SDL_Rect rect, const char* label) {
    RenderContext* ctx = getRenderContext();
    SDL_Renderer* renderer = ctx->renderer;
    int mouse_x = 0;
    int mouse_y = 0;
    SDL_Point mouse_pt;
    bool is_hovered;

    SDL_Color fill = {100, 100, 100, 255};
    SDL_Color fill_active = {140, 140, 140, 255};
    SDL_Color border = {255, 255, 255, 255};
    SDL_Color text = {255, 255, 255, 255};
    ide_shared_theme_button_colors(&fill, &fill_active, &border, &text);

    SDL_GetMouseState(&mouse_x, &mouse_y);
    mouse_pt.x = mouse_x;
    mouse_pt.y = mouse_y;
    is_hovered = SDL_PointInRect(&mouse_pt, &rect);

    if (is_hovered) {
        fill = fill_active;
    }
    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
    SDL_RenderFillRect(renderer, &rect);

    SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
    SDL_RenderDrawRect(renderer, &rect);

    drawTextUTF8WithFontColor(rect.x + 6,
                              rect.y + 4,
                              label,
                              getUIFontByTier(CORE_FONT_TEXT_SIZE_CAPTION),
                              text,
                              false);
}

void drawSelectableText(const SelectableTextOptions* options) {
    if (!options || !options->text || options->text[0] == '\0') return;

    RenderContext* ctx = getRenderContext();
    if (!ctx || !ctx->renderer) return;

    const char* displayText = options->text;
    size_t sourceLen = strlen(displayText);
    if (sourceLen == 0) return;

    int maxWidth = options->maxWidth;
    bool needsClamp = maxWidth > 0;
    size_t visibleLen = sourceLen;
    char* ownedText = NULL;

    if (needsClamp) {
        TTF_Font* font = getActiveFont();
        size_t clampedLen = getTextClampedLengthWithFont(displayText, maxWidth, font);
        if (clampedLen == 0) {
            return;
        }
        visibleLen = clampedLen;
        if (clampedLen < sourceLen) {
            ownedText = (char*)malloc(clampedLen + 1);
            if (!ownedText) {
                drawClippedText(options->x, options->y, displayText, maxWidth);
                return;
            }
            memcpy(ownedText, displayText, clampedLen);
            ownedText[clampedLen] = '\0';
            displayText = ownedText;
        }
    }

    if (needsClamp && ownedText == NULL && visibleLen < sourceLen) {
        drawClippedText(options->x, options->y, displayText, maxWidth);
    } else {
        drawText(options->x, options->y, displayText);
    }

    int textWidth = (visibleLen > 0) ? getTextWidthN(displayText, (int)visibleLen) : 0;
    if (textWidth <= 0 && maxWidth > 0) {
        textWidth = maxWidth;
    }

    TTF_Font* font = getActiveFont();
    int lineHeight = font ? TTF_FontHeight(font) : 20;
    if (lineHeight <= 0) lineHeight = 20;

    TextSelectionRect rect = {
        .bounds = { options->x, options->y, textWidth, lineHeight },
        .line = 0,
        .column_start = 0,
        .column_end = (int)visibleLen,
    };

    void* owner = options->owner ? options->owner : (void*)options->pane;
    UIPaneRole role = options->owner_role;
    if (role == PANE_ROLE_UNKNOWN && options->pane) {
        role = options->pane->role;
    }

    unsigned int flags = options->flags;
    if (flags == TEXT_SELECTION_FLAG_NONE) {
        flags = TEXT_SELECTION_FLAG_SELECTABLE;
    }

    TextSelectionDescriptor desc = {
        .owner = owner,
        .owner_role = role,
        .text = displayText,
        .text_length = visibleLen,
        .rects = &rect,
        .rect_count = 1,
        .flags = flags,
        .copy_handler = NULL,
        .copy_user_data = NULL,
    };
    text_selection_manager_register(&desc);

    if (ownedText) {
        free(ownedText);
    }
}

void pushClipRect(const SDL_Rect* rect) {
    RenderContext* ctx = getRenderContext();
    if (!ctx || !ctx->renderer) return;
    if (s_clip_top < CLIP_STACK_MAX) {
        SDL_Rect current = {0, 0, 0, 0};
        SDL_RenderGetClipRect(ctx->renderer, &current);
        SDL_bool enabled = SDL_RenderIsClipEnabled(ctx->renderer);
        s_clip_stack[s_clip_top] = current;
        s_clip_enabled_stack[s_clip_top] = (enabled == SDL_TRUE);
        s_clip_top++;
    }
    SDL_RenderSetClipRect(ctx->renderer, rect);
}

void popClipRect(void) {
    RenderContext* ctx = getRenderContext();
    if (!ctx || !ctx->renderer) return;
    if (s_clip_top <= 0) {
        SDL_RenderSetClipRect(ctx->renderer, NULL);
        return;
    }
    s_clip_top--;
    if (!s_clip_enabled_stack[s_clip_top]) {
        SDL_RenderSetClipRect(ctx->renderer, NULL);
    } else {
        SDL_RenderSetClipRect(ctx->renderer, &s_clip_stack[s_clip_top]);
    }
}

void render_text_cache_shutdown(void) {
    for (size_t i = 0; i < sizeof(s_text_cache) / sizeof(s_text_cache[0]); ++i) {
        text_cache_destroy_entry(&s_text_cache[i]);
    }
    s_text_cache_tick = 1;
    s_text_cache_bytes = 0;
}
