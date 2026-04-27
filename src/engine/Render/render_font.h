#ifndef RENDER_FONT_H
#define RENDER_FONT_H

#include "core_font.h"

#include <stdbool.h>
#include <SDL2/SDL_ttf.h>

// Font selection enum
typedef enum {
    FONT_MONTSERRAT_BOLD,
    FONT_MONTSERRAT_MEDIUM,
    FONT_MONTSERRAT_MEDIUM_ITALIC,
    FONT_MONTSERRAT_REGULAR,
    FONT_LATO_BOLD,
    FONT_LATO_BOLD_ITALIC,
    FONT_LATO_ITALIC,
    FONT_LATO_REGULAR,
} FontID;


// Font lifecycle
bool initFontSystem();
void shutdownFontSystem();
bool loadFontByID(FontID id);

// Font accessor
TTF_Font* getActiveFont();
TTF_Font* getTerminalFont();
TTF_Font* getUIFontByTier(CoreFontTextSizeTier tier);
int getUIFontPointSizeByTier(CoreFontTextSizeTier tier);
TTF_Font* getRasterizedFontForScale(TTF_Font* base_font, float scale, float* out_raster_scale);

#endif
