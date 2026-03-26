#include "engine/Render/render_font.h"
#include "app/GlobalInfo/runtime_paths.h"
#include "ide/UI/shared_theme_font_adapter.h"
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdbool.h>
#include <limits.h>
#include <string.h>


static TTF_Font* globalFont = NULL;
static TTF_Font* terminalFont = NULL;
static TTF_Font* uiTierFonts[CORE_FONT_TEXT_SIZE_COUNT] = {0};
static int uiTierPoints[CORE_FONT_TEXT_SIZE_COUNT] = {0};

static TTF_Font* open_font_runtime_aware(const char* candidate, int point_size, char* out_path, size_t out_path_cap) {
    char resolved[PATH_MAX];
    const char* load_path = candidate;
    if (!candidate || !candidate[0]) return NULL;
    if (ide_runtime_probe_resource_path(candidate, resolved, sizeof(resolved))) {
        load_path = resolved;
    }
    if (out_path && out_path_cap > 0) {
        strncpy(out_path, load_path, out_path_cap - 1);
        out_path[out_path_cap - 1] = '\0';
    }
    return TTF_OpenFont(load_path, point_size);
}

static void closeUiTierFonts(void) {
    for (int i = 0; i < CORE_FONT_TEXT_SIZE_COUNT; ++i) {
        if (uiTierFonts[i]) {
            if (uiTierFonts[i] == globalFont) {
                globalFont = NULL;
            }
            TTF_CloseFont(uiTierFonts[i]);
            uiTierFonts[i] = NULL;
        }
        uiTierPoints[i] = 0;
    }
}

static bool loadSharedUiTierFonts(void) {
    for (int i = 0; i < CORE_FONT_TEXT_SIZE_COUNT; ++i) {
        char path[256];
        int point = 0;
        TTF_Font* f;
        if (!ide_shared_font_resolve_role(CORE_FONT_ROLE_UI_REGULAR,
                                          (CoreFontTextSizeTier)i,
                                          path,
                                          sizeof(path),
                                          &point)) {
            closeUiTierFonts();
            return false;
        }

        f = TTF_OpenFont(path, point);
        if (!f) {
            fprintf(stderr, "Failed to load shared UI font %s @ %d: %s\n", path, point, TTF_GetError());
            closeUiTierFonts();
            return false;
        }

        uiTierFonts[i] = f;
        uiTierPoints[i] = point;
    }

    globalFont = uiTierFonts[CORE_FONT_TEXT_SIZE_BASIC];
    return globalFont != NULL;
}

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
        char loaded_path[PATH_MAX];
        TTF_Font* f = open_font_runtime_aware(candidates[i], terminalSize, loaded_path, sizeof(loaded_path));
        if (f) {
            TTF_SetFontStyle(f, TTF_STYLE_NORMAL);
            TTF_SetFontHinting(f, TTF_HINTING_LIGHT);
            TTF_SetFontKerning(f, 0);
            terminalFont = f;
            printf("Loaded terminal font: %s\n", loaded_path);
            return;
        }
    }

    fprintf(stderr, "Warning: no monospace terminal font found; falling back to active UI font.\n");
}

static bool loadSharedTerminalFont(void) {
    char path[256];
    int point = 0;
    TTF_Font* f;
    if (!ide_shared_font_resolve_role(CORE_FONT_ROLE_UI_MONO,
                                      CORE_FONT_TEXT_SIZE_BASIC,
                                      path,
                                      sizeof(path),
                                      &point)) {
        return false;
    }

    f = TTF_OpenFont(path, point);
    if (!f) {
        fprintf(stderr, "Failed to load shared terminal font %s @ %d: %s\n", path, point, TTF_GetError());
        return false;
    }
    TTF_SetFontStyle(f, TTF_STYLE_NORMAL);
    TTF_SetFontHinting(f, TTF_HINTING_LIGHT);
    TTF_SetFontKerning(f, 0);
    terminalFont = f;
    return true;
}

bool initFontSystem() {
    if (!TTF_WasInit() && TTF_Init() == -1) {
        fprintf(stderr, "Failed to initialize SDL_ttf: %s\n", TTF_GetError());
        return false;
    }

    closeUiTierFonts();
    if (!loadSharedUiTierFonts()) {
        if (!loadFontByID(FONT_MONTSERRAT_MEDIUM)) {
            return false;
        }
    }

    if (terminalFont) {
        TTF_CloseFont(terminalFont);
        terminalFont = NULL;
    }
    if (!loadSharedTerminalFont()) {
        loadTerminalMonospaceFont();
    }
    return true;
}

void shutdownFontSystem() {
    if (terminalFont) {
        TTF_CloseFont(terminalFont);
        terminalFont = NULL;
    }
    closeUiTierFonts();
    if (globalFont) {
        TTF_CloseFont(globalFont);
        globalFont = NULL;
    }
    if (TTF_WasInit()) {
        TTF_Quit();
    }
}

TTF_Font* getActiveFont() {
    return globalFont;
}

TTF_Font* getTerminalFont() {
    return terminalFont ? terminalFont : globalFont;
}

TTF_Font* getUIFontByTier(CoreFontTextSizeTier tier) {
    if ((int)tier < 0 || tier >= CORE_FONT_TEXT_SIZE_COUNT) {
        return globalFont;
    }
    return uiTierFonts[tier] ? uiTierFonts[tier] : globalFont;
}

int getUIFontPointSizeByTier(CoreFontTextSizeTier tier) {
    if ((int)tier < 0 || tier >= CORE_FONT_TEXT_SIZE_COUNT) {
        return 0;
    }
    return uiTierPoints[tier];
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

    {
        char loaded_path[PATH_MAX];
        globalFont = open_font_runtime_aware(fontPath, fontSize, loaded_path, sizeof(loaded_path));
        if (!globalFont) {
            fprintf(stderr, "Failed to load font: %s\n", TTF_GetError());
            return false;
        }
        printf("Loaded font: %s\n", loaded_path);
    }
    return true;
}
