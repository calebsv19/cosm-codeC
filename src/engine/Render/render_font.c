#include "engine/Render/render_font.h"
#include "app/GlobalInfo/runtime_paths.h"
#include "ide/UI/shared_theme_font_adapter.h"
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <limits.h>
#include <string.h>


static TTF_Font* globalFont = NULL;
static TTF_Font* terminalFont = NULL;
static TTF_Font* uiTierFonts[CORE_FONT_TEXT_SIZE_COUNT] = {0};
static int uiTierPoints[CORE_FONT_TEXT_SIZE_COUNT] = {0};
static char uiTierPaths[CORE_FONT_TEXT_SIZE_COUNT][PATH_MAX];
static char globalFontPath[PATH_MAX];
static int globalFontPoint = 0;
static char terminalFontPath[PATH_MAX];
static int terminalFontPoint = 0;

enum {
    FONT_SOURCE_CAPACITY = CORE_FONT_TEXT_SIZE_COUNT + 8,
    FONT_RASTER_CACHE_CAPACITY = 32
};

typedef struct {
    TTF_Font* font;
    char path[PATH_MAX];
    int point_size;
    int kerning_enabled;
} FontSourceEntry;

typedef struct {
    TTF_Font* base_font;
    int requested_point_size;
    TTF_Font* raster_font;
} FontRasterCacheEntry;

static FontSourceEntry g_font_sources[FONT_SOURCE_CAPACITY];
static FontRasterCacheEntry g_raster_cache[FONT_RASTER_CACHE_CAPACITY];

static void clear_font_sources(void) {
    memset(g_font_sources, 0, sizeof(g_font_sources));
}

static void clear_raster_font_cache(void) {
    for (int i = 0; i < FONT_RASTER_CACHE_CAPACITY; ++i) {
        if (g_raster_cache[i].raster_font) {
            TTF_CloseFont(g_raster_cache[i].raster_font);
        }
        g_raster_cache[i] = (FontRasterCacheEntry){0};
    }
}

static void configure_font(TTF_Font* font, int kerning_enabled) {
    if (!font) return;
    TTF_SetFontStyle(font, TTF_STYLE_NORMAL);
    TTF_SetFontHinting(font, TTF_HINTING_LIGHT);
    TTF_SetFontKerning(font, kerning_enabled ? 1 : 0);
}

static void register_font_source(TTF_Font* font, const char* path, int point_size, int kerning_enabled) {
    if (!font || !path || !path[0] || point_size <= 0) return;
    for (int i = 0; i < FONT_SOURCE_CAPACITY; ++i) {
        if (g_font_sources[i].font == font) {
            strncpy(g_font_sources[i].path, path, sizeof(g_font_sources[i].path) - 1);
            g_font_sources[i].path[sizeof(g_font_sources[i].path) - 1] = '\0';
            g_font_sources[i].point_size = point_size;
            g_font_sources[i].kerning_enabled = kerning_enabled ? 1 : 0;
            return;
        }
    }
    for (int i = 0; i < FONT_SOURCE_CAPACITY; ++i) {
        if (!g_font_sources[i].font) {
            g_font_sources[i].font = font;
            strncpy(g_font_sources[i].path, path, sizeof(g_font_sources[i].path) - 1);
            g_font_sources[i].path[sizeof(g_font_sources[i].path) - 1] = '\0';
            g_font_sources[i].point_size = point_size;
            g_font_sources[i].kerning_enabled = kerning_enabled ? 1 : 0;
            return;
        }
    }
}

static const FontSourceEntry* find_font_source(TTF_Font* font) {
    if (!font) return NULL;
    for (int i = 0; i < FONT_SOURCE_CAPACITY; ++i) {
        if (g_font_sources[i].font == font && g_font_sources[i].path[0]) {
            return &g_font_sources[i];
        }
    }
    return NULL;
}

static void rebuild_font_sources(void) {
    clear_font_sources();
    for (int i = 0; i < CORE_FONT_TEXT_SIZE_COUNT; ++i) {
        if (uiTierFonts[i] && uiTierPaths[i][0] && uiTierPoints[i] > 0) {
            register_font_source(uiTierFonts[i], uiTierPaths[i], uiTierPoints[i], 1);
        }
    }
    if (globalFont && globalFontPath[0] && globalFontPoint > 0) {
        register_font_source(globalFont, globalFontPath, globalFontPoint, 1);
    }
    if (terminalFont && terminalFontPath[0] && terminalFontPoint > 0) {
        register_font_source(terminalFont, terminalFontPath, terminalFontPoint, 0);
    }
}

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
    clear_raster_font_cache();
    for (int i = 0; i < CORE_FONT_TEXT_SIZE_COUNT; ++i) {
        if (uiTierFonts[i]) {
            if (uiTierFonts[i] == globalFont) {
                globalFont = NULL;
            }
            TTF_CloseFont(uiTierFonts[i]);
            uiTierFonts[i] = NULL;
        }
        uiTierPoints[i] = 0;
        uiTierPaths[i][0] = '\0';
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

        configure_font(f, 1);
        uiTierFonts[i] = f;
        uiTierPoints[i] = point;
        strncpy(uiTierPaths[i], path, sizeof(uiTierPaths[i]) - 1);
        uiTierPaths[i][sizeof(uiTierPaths[i]) - 1] = '\0';
    }

    globalFont = uiTierFonts[CORE_FONT_TEXT_SIZE_BASIC];
    if (globalFont) {
        strncpy(globalFontPath, uiTierPaths[CORE_FONT_TEXT_SIZE_BASIC], sizeof(globalFontPath) - 1);
        globalFontPath[sizeof(globalFontPath) - 1] = '\0';
        globalFontPoint = uiTierPoints[CORE_FONT_TEXT_SIZE_BASIC];
    } else {
        globalFontPath[0] = '\0';
        globalFontPoint = 0;
    }
    return globalFont != NULL;
}

static void loadTerminalMonospaceFont(void) {
    clear_raster_font_cache();
    if (terminalFont) {
        TTF_CloseFont(terminalFont);
        terminalFont = NULL;
    }
    terminalFontPath[0] = '\0';
    terminalFontPoint = 0;

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
            configure_font(f, 0);
            terminalFont = f;
            strncpy(terminalFontPath, loaded_path, sizeof(terminalFontPath) - 1);
            terminalFontPath[sizeof(terminalFontPath) - 1] = '\0';
            terminalFontPoint = terminalSize;
            printf("Loaded terminal font: %s\n", loaded_path);
            return;
        }
    }

    fprintf(stderr, "Warning: no monospace terminal font found; falling back to active UI font.\n");
}

static bool loadSharedTerminalFont(void) {
    clear_raster_font_cache();
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
    configure_font(f, 0);
    terminalFont = f;
    strncpy(terminalFontPath, path, sizeof(terminalFontPath) - 1);
    terminalFontPath[sizeof(terminalFontPath) - 1] = '\0';
    terminalFontPoint = point;
    return true;
}

bool initFontSystem() {
    if (!TTF_WasInit() && TTF_Init() == -1) {
        fprintf(stderr, "Failed to initialize SDL_ttf: %s\n", TTF_GetError());
        return false;
    }

    clear_raster_font_cache();
    clear_font_sources();
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
    rebuild_font_sources();
    return true;
}

void shutdownFontSystem() {
    clear_raster_font_cache();
    clear_font_sources();
    if (terminalFont) {
        TTF_CloseFont(terminalFont);
        terminalFont = NULL;
    }
    terminalFontPath[0] = '\0';
    terminalFontPoint = 0;
    closeUiTierFonts();
    if (globalFont) {
        TTF_CloseFont(globalFont);
        globalFont = NULL;
    }
    globalFontPath[0] = '\0';
    globalFontPoint = 0;
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

TTF_Font* getRasterizedFontForScale(TTF_Font* base_font, float scale, float* out_raster_scale) {
    const FontSourceEntry* src;
    int requested_point_size;
    int slot = -1;

    if (out_raster_scale) {
        *out_raster_scale = 1.0f;
    }
    if (!base_font) {
        return NULL;
    }
    if (scale < 1.0f) {
        scale = 1.0f;
    }
    if (scale > 4.0f) {
        scale = 4.0f;
    }

    src = find_font_source(base_font);
    if (!src || src->point_size <= 0) {
        return base_font;
    }

    requested_point_size = (int)lroundf((float)src->point_size * scale);
    if (requested_point_size <= src->point_size) {
        return base_font;
    }

    for (int i = 0; i < FONT_RASTER_CACHE_CAPACITY; ++i) {
        if (g_raster_cache[i].base_font == base_font &&
            g_raster_cache[i].requested_point_size == requested_point_size &&
            g_raster_cache[i].raster_font) {
            if (out_raster_scale) {
                *out_raster_scale = (float)requested_point_size / (float)src->point_size;
            }
            return g_raster_cache[i].raster_font;
        }
    }

    for (int i = 0; i < FONT_RASTER_CACHE_CAPACITY; ++i) {
        if (!g_raster_cache[i].raster_font) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        slot = 0;
    }
    if (g_raster_cache[slot].raster_font) {
        TTF_CloseFont(g_raster_cache[slot].raster_font);
    }
    g_raster_cache[slot] = (FontRasterCacheEntry){0};

    g_raster_cache[slot].raster_font = TTF_OpenFont(src->path, requested_point_size);
    if (!g_raster_cache[slot].raster_font) {
        fprintf(stderr, "Failed to load raster font %s @ %d: %s\n",
                src->path,
                requested_point_size,
                TTF_GetError());
        return base_font;
    }
    configure_font(g_raster_cache[slot].raster_font, src->kerning_enabled);
    g_raster_cache[slot].base_font = base_font;
    g_raster_cache[slot].requested_point_size = requested_point_size;
    if (out_raster_scale) {
        *out_raster_scale = (float)requested_point_size / (float)src->point_size;
    }
    return g_raster_cache[slot].raster_font;
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

    clear_raster_font_cache();
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
        configure_font(globalFont, 1);
        strncpy(globalFontPath, loaded_path, sizeof(globalFontPath) - 1);
        globalFontPath[sizeof(globalFontPath) - 1] = '\0';
        globalFontPoint = fontSize;
        rebuild_font_sources();
        printf("Loaded font: %s\n", loaded_path);
    }
    return true;
}
