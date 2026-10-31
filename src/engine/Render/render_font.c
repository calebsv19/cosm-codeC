#include "engine/Render/render_font.h"
#include "engine/Render/render_text_helpers.h"
#include "app/GlobalInfo/runtime_paths.h"
#include "ide/UI/shared_theme_font_adapter.h"
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <limits.h>
#include <string.h>


typedef struct {
    TTF_Font* globalFont;
    TTF_Font* terminalFont;
    TTF_Font* uiTierFonts[CORE_FONT_TEXT_SIZE_COUNT];
    int uiTierPoints[CORE_FONT_TEXT_SIZE_COUNT];
    char uiTierPaths[CORE_FONT_TEXT_SIZE_COUNT][PATH_MAX];
    char globalFontPath[PATH_MAX];
    int globalFontPoint;
    char terminalFontPath[PATH_MAX];
    int terminalFontPoint;
} FontSet;

static uint64_t g_font_revision;
uint64_t getFontSystemRevision(void) { return g_font_revision; }

static FontSet g_fonts;
static FontSet g_preview_baseline;
static bool g_preview_active;

static bool font_set_contains(const FontSet* set, TTF_Font* font) {
    if (!set || !font) return false;
    if (set->globalFont == font || set->terminalFont == font) return true;
    for (int i = 0; i < CORE_FONT_TEXT_SIZE_COUNT; ++i)
        if (set->uiTierFonts[i] == font) return true;
    return false;
}

static void close_unpinned_font(TTF_Font* font) {
    if (font && !(g_preview_active && font_set_contains(&g_preview_baseline, font)))
        TTF_CloseFont(font);
}

/* A preview pins entry handles until Apply or Cancel, including partial aliases. */
static void close_font_set_except(FontSet* set, const FontSet* keep) {
    if (!font_set_contains(keep, set->terminalFont)) close_unpinned_font(set->terminalFont);
    for (int i = 0; i < CORE_FONT_TEXT_SIZE_COUNT; ++i) {
        if (set->uiTierFonts[i] == set->globalFont) set->globalFont = NULL;
        if (!font_set_contains(keep, set->uiTierFonts[i])) close_unpinned_font(set->uiTierFonts[i]);
    }
    if (!font_set_contains(keep, set->globalFont)) close_unpinned_font(set->globalFont);
    memset(set, 0, sizeof(*set));
}

static void close_font_set(FontSet* set) { close_font_set_except(set, NULL); }

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
    ++g_font_revision;
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
        if (g_fonts.uiTierFonts[i] && g_fonts.uiTierPaths[i][0] && g_fonts.uiTierPoints[i] > 0) {
            register_font_source(g_fonts.uiTierFonts[i], g_fonts.uiTierPaths[i], g_fonts.uiTierPoints[i], 1);
        }
    }
    if (g_fonts.globalFont && g_fonts.globalFontPath[0] && g_fonts.globalFontPoint > 0) {
        register_font_source(g_fonts.globalFont, g_fonts.globalFontPath, g_fonts.globalFontPoint, 1);
    }
    if (g_fonts.terminalFont && g_fonts.terminalFontPath[0] && g_fonts.terminalFontPoint > 0) {
        register_font_source(g_fonts.terminalFont, g_fonts.terminalFontPath, g_fonts.terminalFontPoint, 0);
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
        if (g_fonts.uiTierFonts[i]) {
            if (g_fonts.uiTierFonts[i] == g_fonts.globalFont) {
                g_fonts.globalFont = NULL;
            }
            close_unpinned_font(g_fonts.uiTierFonts[i]);
            g_fonts.uiTierFonts[i] = NULL;
        }
        g_fonts.uiTierPoints[i] = 0;
        g_fonts.uiTierPaths[i][0] = '\0';
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
        g_fonts.uiTierFonts[i] = f;
        g_fonts.uiTierPoints[i] = point;
        strncpy(g_fonts.uiTierPaths[i], path, sizeof(g_fonts.uiTierPaths[i]) - 1);
        g_fonts.uiTierPaths[i][sizeof(g_fonts.uiTierPaths[i]) - 1] = '\0';
    }

    g_fonts.globalFont = g_fonts.uiTierFonts[CORE_FONT_TEXT_SIZE_BASIC];
    if (g_fonts.globalFont) {
        strncpy(g_fonts.globalFontPath, g_fonts.uiTierPaths[CORE_FONT_TEXT_SIZE_BASIC], sizeof(g_fonts.globalFontPath) - 1);
        g_fonts.globalFontPath[sizeof(g_fonts.globalFontPath) - 1] = '\0';
        g_fonts.globalFontPoint = g_fonts.uiTierPoints[CORE_FONT_TEXT_SIZE_BASIC];
    } else {
        g_fonts.globalFontPath[0] = '\0';
        g_fonts.globalFontPoint = 0;
    }
    return g_fonts.globalFont != NULL;
}

static void loadTerminalMonospaceFont(void) {
    clear_raster_font_cache();
    if (g_fonts.terminalFont) {
        close_unpinned_font(g_fonts.terminalFont);
        g_fonts.terminalFont = NULL;
    }
    g_fonts.terminalFontPath[0] = '\0';
    g_fonts.terminalFontPoint = 0;

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
            g_fonts.terminalFont = f;
            strncpy(g_fonts.terminalFontPath, loaded_path, sizeof(g_fonts.terminalFontPath) - 1);
            g_fonts.terminalFontPath[sizeof(g_fonts.terminalFontPath) - 1] = '\0';
            g_fonts.terminalFontPoint = terminalSize;
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
    g_fonts.terminalFont = f;
    strncpy(g_fonts.terminalFontPath, path, sizeof(g_fonts.terminalFontPath) - 1);
    g_fonts.terminalFontPath[sizeof(g_fonts.terminalFontPath) - 1] = '\0';
    g_fonts.terminalFontPoint = point;
    return true;
}

bool initFontSystem() {
    if (!TTF_WasInit() && TTF_Init() == -1) {
        fprintf(stderr, "Failed to initialize SDL_ttf: %s\n", TTF_GetError());
        return false;
    }

    /* Stage new handles while the current set remains alive. Failed reloads
       restore it; allocator pointer reuse cannot alias old handles during load. */
    FontSet previous = g_fonts;
    memset(&g_fonts, 0, sizeof(g_fonts));
    clear_raster_font_cache();
    clear_font_sources();
    if (!loadSharedUiTierFonts() &&
        (previous.globalFont || !loadFontByID(FONT_MONTSERRAT_MEDIUM))) {
        close_font_set(&g_fonts);
        g_fonts = previous;
        rebuild_font_sources();
        return false;
    }
    if (!loadSharedTerminalFont()) loadTerminalMonospaceFont();
    close_font_set(&previous);
    rebuild_font_sources();
    render_text_measurement_cache_reset();
    return true;
}

bool reloadFontSystem() {
    return initFontSystem();
}

bool beginFontPreview(void) {
    if (g_preview_active || !g_fonts.globalFont) return false;
    g_preview_baseline = g_fonts;
    g_preview_active = true;
    return true;
}

void acceptFontPreview(void) {
    if (!g_preview_active) return;
    g_preview_active = false;
    close_font_set_except(&g_preview_baseline, &g_fonts);
}

bool cancelFontPreview(void) {
    if (!g_preview_active) return false;
    clear_raster_font_cache();
    close_font_set(&g_fonts);
    g_fonts = g_preview_baseline;
    memset(&g_preview_baseline, 0, sizeof(g_preview_baseline));
    g_preview_active = false;
    rebuild_font_sources();
    render_text_measurement_cache_reset();
    return true;
}

void shutdownFontSystem() {
    clear_raster_font_cache();
    clear_font_sources();
    close_font_set(&g_fonts);
    g_preview_active = false;
    close_font_set(&g_preview_baseline);
    render_text_measurement_cache_reset();
    if (TTF_WasInit()) {
        TTF_Quit();
    }
}

TTF_Font* getActiveFont() {
    return g_fonts.globalFont;
}

TTF_Font* getTerminalFont() {
    return g_fonts.terminalFont ? g_fonts.terminalFont : g_fonts.globalFont;
}

TTF_Font* getUIFontByTier(CoreFontTextSizeTier tier) {
    if ((int)tier < 0 || tier >= CORE_FONT_TEXT_SIZE_COUNT) {
        return g_fonts.globalFont;
    }
    return g_fonts.uiTierFonts[tier] ? g_fonts.uiTierFonts[tier] : g_fonts.globalFont;
}

int getUIFontPointSizeByTier(CoreFontTextSizeTier tier) {
    if ((int)tier < 0 || tier >= CORE_FONT_TEXT_SIZE_COUNT) {
        return 0;
    }
    return g_fonts.uiTierPoints[tier];
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

    char loaded_path[PATH_MAX];
    TTF_Font* replacement = open_font_runtime_aware(fontPath, fontSize, loaded_path, sizeof(loaded_path));
    if (!replacement) {
        fprintf(stderr, "Failed to load font: %s\n", TTF_GetError());
        return false;
    }
    configure_font(replacement, 1);
    clear_raster_font_cache();
    for (int i = 0; i < CORE_FONT_TEXT_SIZE_COUNT; ++i) {
        if (g_fonts.globalFont && g_fonts.uiTierFonts[i] == g_fonts.globalFont) {
            g_fonts.uiTierFonts[i] = replacement;
            g_fonts.uiTierPoints[i] = fontSize;
            snprintf(g_fonts.uiTierPaths[i], sizeof(g_fonts.uiTierPaths[i]), "%s", loaded_path);
        }
    }
    if (g_fonts.globalFont) close_unpinned_font(g_fonts.globalFont);
    g_fonts.globalFont = replacement;
    snprintf(g_fonts.globalFontPath, sizeof(g_fonts.globalFontPath), "%s", loaded_path);
    g_fonts.globalFontPoint = fontSize;
    rebuild_font_sources();
    render_text_measurement_cache_reset();
    return true;
}

#include "kit_render_native_font.h"
int measureNativeFontText(TTF_Font* font, const char* text, int* width, int* height) {
    const FontSourceEntry* source = find_font_source(font);
    return source && kit_render_native_font_measure(source->path,source->point_size,text,width,height);
}
int measureNativeFallbackText(TTF_Font* font, const char* text, int* width, int* height) {
    return kit_render_native_font_needs_fallback(font,text) && measureNativeFontText(font,text,width,height);
}
SDL_Surface* rasterizeNativeFallbackText(TTF_Font* font, const char* text, SDL_Color color, float scale) {
    const FontSourceEntry* source = find_font_source(font);
    if (!source || !kit_render_native_font_needs_fallback(font,text)) return NULL;
    return kit_render_native_font_rasterize(source->path,source->point_size,text,color,scale);
}
