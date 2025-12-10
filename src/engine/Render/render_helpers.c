#include "engine/Render/render_helpers.h"
#include "engine/Render/render_text_helpers.h"
#include "engine/Render/render_pipeline.h"  // for getRenderContext
#include "engine/Render/render_font.h"      // for getActiveFont
#include "core/TextSelection/text_selection_manager.h"
#include "ide/Panes/PaneInfo/pane.h"

#include <SDL2/SDL_ttf.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#if !USE_VULKAN
#define CLIP_STACK_MAX 16
static SDL_Rect s_clip_stack[CLIP_STACK_MAX];
static bool s_clip_enabled_stack[CLIP_STACK_MAX];
static int s_clip_top = 0;
#endif

void drawText(int x, int y, const char* text) {
    drawTextWithFont(x, y, text, getActiveFont());
}

void drawTextWithFont(int x, int y, const char* text, TTF_Font* font) {
    if (!text || text[0] == '\0') return;
    if (!font) return;

    RenderContext* ctx = getRenderContext();
    if (!ctx || !ctx->renderer) return;

    SDL_Renderer* renderer = ctx->renderer;

    SDL_Color color = {255, 255, 255, 255};
    SDL_Surface* surface = TTF_RenderText_Blended(font, text, color);
    if (!surface) return;

    SDL_Rect dst = {x, y, surface->w, surface->h};

#if USE_VULKAN
    VkRendererTexture vkTexture = {0};
    SDL_Rect srcRect = {0, 0, surface->w, surface->h};
    if (vk_renderer_upload_sdl_surface_with_filter(renderer, surface, &vkTexture,
                                                   VK_FILTER_NEAREST) == VK_SUCCESS) {
        vk_renderer_draw_texture(renderer, &vkTexture, &srcRect, &dst);
        vk_renderer_queue_texture_destroy(renderer, &vkTexture);
    }
#else
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture) {
        SDL_RenderCopy(renderer, texture, NULL, &dst);
        SDL_DestroyTexture(texture);
    }
#endif

    SDL_FreeSurface(surface);
}

void drawClippedText(int x, int y, const char* text, int maxWidth) {
    if (!text || text[0] == '\0' || maxWidth <= 0) return;

    size_t cutoff = getTextClampedLength(text, maxWidth);
    if (cutoff == 0) return;

    char temp[1024] = {0};
    size_t copyLen = cutoff;
    if (copyLen >= sizeof(temp)) {
        copyLen = sizeof(temp) - 1;
    }
    strncpy(temp, text, copyLen);
    temp[copyLen] = '\0';

    drawText(x, y, temp);
}


void renderButton(UIPane* pane, SDL_Rect rect, const char* label) {
    RenderContext* ctx = getRenderContext();
    SDL_Renderer* renderer = ctx->renderer;

    SDL_Color white = {255, 255, 255, 255};
    SDL_Color gray = {100, 100, 100, 255};

    SDL_SetRenderDrawColor(renderer, gray.r, gray.g, gray.b, 255);
    SDL_RenderFillRect(renderer, &rect);

    SDL_SetRenderDrawColor(renderer, white.r, white.g, white.b, 255);
    SDL_RenderDrawRect(renderer, &rect);

    SelectableTextOptions opts = {
        .pane = pane,
        .owner = pane ? (void*)pane : NULL,
        .owner_role = pane ? pane->role : PANE_ROLE_UNKNOWN,
        .x = rect.x + 6,
        .y = rect.y + 4,
        .maxWidth = rect.w - 12,
        .text = label,
        .flags = TEXT_SELECTION_FLAG_SELECTABLE,
    };
    drawSelectableText(&opts);
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
        size_t clampedLen = getTextClampedLength(displayText, maxWidth);
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
#if USE_VULKAN
    (void)rect;
#else
    if (s_clip_top < CLIP_STACK_MAX) {
        SDL_Rect current = {0, 0, 0, 0};
        SDL_RenderGetClipRect(ctx->renderer, &current);
        SDL_bool enabled = SDL_RenderIsClipEnabled(ctx->renderer);
        s_clip_stack[s_clip_top] = current;
        s_clip_enabled_stack[s_clip_top] = (enabled == SDL_TRUE);
        s_clip_top++;
    }
    SDL_RenderSetClipRect(ctx->renderer, rect);
#endif
}

void popClipRect(void) {
    RenderContext* ctx = getRenderContext();
    if (!ctx || !ctx->renderer) return;
#if USE_VULKAN
    (void)ctx;
#else
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
#endif
}
