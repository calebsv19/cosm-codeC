#ifndef RENDER_FONT_H
#define RENDER_FONT_H

#include <stdbool.h>
#include <SDL2/SDL_ttf.h>

// Font selection enum
typedef enum {
    FONT_MONTSERRAT_MEDIUM,
    FONT_MONTSERRAT_REGULAR,
    FONT_INTER_REGULAR,
    FONT_LATO_REGULAR,
    FONT_LATO_BOLD,
    FONT_ROBOTO_LIGHT,
    FONT_IBM_CONDENSED
} FontID;

// Font lifecycle
bool initFontSystem();
void shutdownFontSystem();
bool loadFontByID(FontID id);

// Font accessor
TTF_Font* getActiveFont();

#endif

