#include "engine/Render/render_helpers.h"
#include "engine/Render/render_text_helpers.h"
#include "engine/Render/render_pipeline.h"  // for getRenderContext
#include "engine/Render/render_font.h"      // for getActiveFont

#include <SDL2/SDL_ttf.h>
#include <string.h>

void drawText(int x, int y, const char* text) {
    TTF_Font* font = getActiveFont();     
    if (!font) return;

    RenderContext* ctx = getRenderContext();
    SDL_Renderer* renderer = ctx->renderer;

    SDL_Color color = {255, 255, 255, 255};
    SDL_Surface* surface = TTF_RenderText_Blended(font, text, color);
    if (!surface) return;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_Rect dst = {x, y, surface->w, surface->h};
    SDL_RenderCopy(renderer, texture, NULL, &dst);

    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
}

void drawClippedText(int x, int y, const char* text, int maxWidth) {
    int len = strlen(text);
    char temp[1024]; 
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

