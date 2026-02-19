#include "engine/Render/render_text_helpers.h"
#include "engine/Render/render_font.h"  		// for getActiveFont()

#include <SDL2/SDL_ttf.h>
#include <string.h>

int getTextWidthWithFont(const char* text, TTF_Font* font) {
    if (!text || text[0] == '\0') return 0;
    if (!font) return 0;

    int w = 0, h = 0;
    if (TTF_SizeText(font, text, &w, &h) != 0) {
        return 0;  // Fallback on error
    }

    return w;
}

int getTextWidth(const char* text) {
    return getTextWidthWithFont(text, getActiveFont());
}

int getTextWidthNWithFont(const char* text, int n, TTF_Font* font) {
    if (!text || n <= 0) return 0;
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

int getTextWidthN(const char* text, int n) {
    return getTextWidthNWithFont(text, n, getActiveFont());
}

int getTextWidthUTF8WithFont(const char* text, TTF_Font* font) {
    if (!text || text[0] == '\0') return 0;
    if (!font) return 0;

    int w = 0, h = 0;
    if (TTF_SizeUTF8(font, text, &w, &h) != 0) {
        return 0;
    }
    return w;
}

size_t getTextClampedLength(const char* text, int maxWidth) {
    if (!text) return 0;
    if (maxWidth <= 0) return strlen(text);

    size_t len = strlen(text);
    if (len == 0) return 0;

    size_t cutoff = len;
    for (size_t i = 1; i <= len; ++i) {
        int width = getTextWidthN(text, (int)i);
        if (width > maxWidth) {
            cutoff = (i > 0) ? i - 1 : 0;
            break;
        }
    }
    return cutoff;
}
