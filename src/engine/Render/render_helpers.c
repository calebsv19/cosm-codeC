#include "engine/Render/render_helpers.h"
#include "engine/Render/render_text_helpers.h"
#include "engine/Render/render_pipeline.h"  // for getRenderContext
#include "engine/Render/render_font.h"      // for getActiveFont

#include <SDL2/SDL_ttf.h>
#include <string.h>

void drawText(int x, int y, const char* text) {
    if (!text || text[0] == '\0') return;

    TTF_Font* font = getActiveFont();
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
    if (vk_renderer_upload_sdl_surface(renderer, surface, &vkTexture) == VK_SUCCESS) {
        vk_renderer_draw_texture(renderer, &vkTexture, NULL, &dst);
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

    int len = strnlen(text, 1023);  // Prevent overflow
    char temp[1024] = {0};
    int cutoff = len;

    for (int i = 1; i <= len; i++) {
        strncpy(temp, text, i);
        temp[i] = '\0';

        if (getTextWidth(temp) > maxWidth) {
            cutoff = i - 1;
            break;
        }
    }

    strncpy(temp, text, cutoff);
    temp[cutoff] = '\0';

    drawText(x, y, temp);
}


void renderButton(SDL_Rect rect, const char* label) {
    RenderContext* ctx = getRenderContext();
    SDL_Renderer* renderer = ctx->renderer;

    SDL_Color white = {255, 255, 255, 255};
    SDL_Color gray = {100, 100, 100, 255};

    SDL_SetRenderDrawColor(renderer, gray.r, gray.g, gray.b, 255);
    SDL_RenderFillRect(renderer, &rect);

    SDL_SetRenderDrawColor(renderer, white.r, white.g, white.b, 255);
    SDL_RenderDrawRect(renderer, &rect);

    drawText(rect.x + 6, rect.y + 4, label);
}
