#include "engine/Render/render_text_helpers.h"
#include "engine/Render/render_font.h"  		// for getActiveFont()

#include <SDL2/SDL_ttf.h>
#include <string.h>

int getTextWidth(const char* text) {
    if (!text || text[0] == '\0') return 0;

    TTF_Font* font = getActiveFont();
    if (!font) return 0;

    int w = 0, h = 0;
    if (TTF_SizeText(font, text, &w, &h) != 0) {
        return 0;  // Fallback on error
    }

    return w;
}


int getTextWidthN(const char* text, int n) {
    if (!text || n <= 0) return 0;

    TTF_Font* font = getActiveFont();
    if (!font) return 0;

    int len = strnlen(text, n);
    if (len == 0) return 0;

    // Ensure we never overflow temp
    if (len >= 1024) len = 1023;

    char temp[1024];
    strncpy(temp, text, len);
    temp[len] = '\0';

    int w = 0, h = 0;
    if (TTF_SizeText(font, temp, &w, &h) != 0) {
        return 0;
    }

    return w;
}

