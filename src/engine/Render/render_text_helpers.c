#include "engine/Render/render_text_helpers.h"
#include "engine/Render/render_font.h"  		// for getActiveFont()

#include <SDL2/SDL_ttf.h>
#include <string.h>

int getTextWidth(const char* text) {
    TTF_Font* font = getActiveFont();  
    if (!font) return 0;

    int w = 0, h = 0;
    TTF_SizeText(font, text, &w, &h);
    return w;
}

int getTextWidthN(const char* text, int n) {
    TTF_Font* font = getActiveFont();
    if (!font) return 0;

    char temp[1024];
    strncpy(temp, text, n);
    temp[n] = '\0';

    int w = 0, h = 0;
    TTF_SizeText(font, temp, &w, &h);
    return w;
}

