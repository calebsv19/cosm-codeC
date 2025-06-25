#include "engine/Render/render_font.h"
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdbool.h>


static TTF_Font* globalFont = NULL;

bool initFontSystem() {
    if (TTF_Init() == -1) {
        fprintf(stderr, "Failed to initialize SDL_ttf: %s\n", TTF_GetError());
        return false;
    }
    return loadFontByID(FONT_MONTSERRAT_REGULAR);
}

void shutdownFontSystem() {
    if (globalFont) {
        TTF_CloseFont(globalFont);
        globalFont = NULL;
    }
    TTF_Quit();
}

TTF_Font* getActiveFont() {
    return globalFont;
}

bool loadFontByID(FontID id) {
    const char* fontPath = NULL;
    int fontSize = 14;

    switch (id) {
        case FONT_MONTSERRAT_MEDIUM:
            fontPath = "include/fonts/Montserrat/static/Montserrat-Medium.ttf";
            break;
        case FONT_MONTSERRAT_REGULAR:
            fontPath = "include/fonts/Montserrat/static/Montserrat-Regular.ttf";
            break;
        case FONT_INTER_REGULAR:
            fontPath = "include/fonts/Inter/static/Inter_18pt-Regular.ttf";
            break;
        case FONT_LATO_REGULAR:
            fontPath = "include/fonts/Lato/Lato-Regular.ttf";
            break;
        case FONT_LATO_BOLD:
            fontPath = "include/fonts/Lato/Lato-Bold.ttf";
            break;
        case FONT_ROBOTO_LIGHT:
            fontPath = "include/fonts/Roboto/static/Roboto-Light.ttf";
            break;
        case FONT_IBM_CONDENSED:
            fontPath = "include/fonts/IBM_Plex_Sans/static/IBMPlexSans_Condensed-Regular.ttf";
            fontSize = 20;
            break;
        default:
            fprintf(stderr, "Unknown font ID\n");
            return false;
    }

    if (globalFont) {
        TTF_CloseFont(globalFont);
    }

    globalFont = TTF_OpenFont(fontPath, fontSize);
    if (!globalFont) {
        fprintf(stderr, "Failed to load font: %s\n", TTF_GetError());
        return false;
    }

    printf("Loaded font: %s\n", fontPath);
    return true;
}

