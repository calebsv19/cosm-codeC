#include "engine/Render/render_font.h"
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdbool.h>


static TTF_Font* globalFont = NULL;
static TTF_Font* terminalFont = NULL;

static void loadTerminalMonospaceFont(void) {
    if (terminalFont) {
        TTF_CloseFont(terminalFont);
        terminalFont = NULL;
    }

    const char* candidates[] = {
        "include/fonts/JetBrainsMono/JetBrainsMono-Regular.ttf",
        "include/fonts/FiraCode/FiraCode-Regular.ttf",
        "/System/Library/Fonts/Menlo.ttc",
        "/System/Library/Fonts/Supplemental/Courier New.ttf",
        "/Library/Fonts/Courier New.ttf",
    };
    const int candidateCount = (int)(sizeof(candidates) / sizeof(candidates[0]));
    const int terminalSize = 13;

    for (int i = 0; i < candidateCount; ++i) {
        TTF_Font* f = TTF_OpenFont(candidates[i], terminalSize);
        if (f) {
            TTF_SetFontStyle(f, TTF_STYLE_NORMAL);
            TTF_SetFontHinting(f, TTF_HINTING_LIGHT);
            TTF_SetFontKerning(f, 0);
            terminalFont = f;
            printf("Loaded terminal font: %s\n", candidates[i]);
            return;
        }
    }

    fprintf(stderr, "Warning: no monospace terminal font found; falling back to active UI font.\n");
}

bool initFontSystem() {
    if (TTF_Init() == -1) {
        fprintf(stderr, "Failed to initialize SDL_ttf: %s\n", TTF_GetError());
        return false;
    }
    if (!loadFontByID(FONT_MONTSERRAT_MEDIUM)) {
        return false;
    }
    loadTerminalMonospaceFont();
    return true;
}

void shutdownFontSystem() {
    if (terminalFont) {
        TTF_CloseFont(terminalFont);
        terminalFont = NULL;
    }
    if (globalFont) {
        TTF_CloseFont(globalFont);
        globalFont = NULL;
    }
    TTF_Quit();
}

TTF_Font* getActiveFont() {
    return globalFont;
}

TTF_Font* getTerminalFont() {
    return terminalFont ? terminalFont : globalFont;
}

bool loadFontByID(FontID id) {
    const char* fontPath = NULL;
    int fontSize = 14;

    switch (id) {
        case FONT_MONTSERRAT_BOLD:
            fontPath = "include/fonts/Montserrat/Montserrat-Bold.ttf";
            break;
        case FONT_MONTSERRAT_MEDIUM:
            fontPath = "include/fonts/Montserrat/Montserrat-Medium.ttf";
            break;
        case FONT_MONTSERRAT_MEDIUM_ITALIC:
            fontPath = "include/fonts/Montserrat/Montserrat-MediumItalic.ttf";
            break;
        case FONT_MONTSERRAT_REGULAR:
            fontPath = "include/fonts/Montserrat/Montserrat-Regular.ttf";
            break;	

        case FONT_LATO_BOLD:
            fontPath = "include/fonts/Lato/Lato-Bold.ttf";
            break;
        case FONT_LATO_BOLD_ITALIC:
            fontPath = "include/fonts/Lato/Lato-BoldItalic.ttf";
            break;
        case FONT_LATO_ITALIC:
            fontPath = "include/fonts/Lato/Lato-Italic.ttf";
            break;
        case FONT_LATO_REGULAR:
            fontPath = "include/fonts/Lato/Lato-Regular.ttf";
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
