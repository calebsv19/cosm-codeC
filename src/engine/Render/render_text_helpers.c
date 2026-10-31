#include "engine/Render/render_text_helpers.h"
#include "engine/Render/render_text_pixels.h"
#include "engine/Render/render_pipeline.h"
#include "engine/Render/render_font.h"  		// for getActiveFont()

#include <SDL2/SDL_ttf.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const TTF_Font* font;
    uint64_t font_revision;
    float scale_x, scale_y;
    const char* text_ptr;
    int max_width;
    size_t text_len;
    unsigned int text_fingerprint;
    size_t cutoff;
    int valid;
} ClampCacheEntry;

#define CLAMP_CACHE_SIZE 256
static ClampCacheEntry s_clamp_cache[CLAMP_CACHE_SIZE];

void render_text_measurement_cache_reset(void) {
    memset(s_clamp_cache, 0, sizeof(s_clamp_cache));
}

static unsigned int clamp_cache_fingerprint(const char* text, size_t len) {
    unsigned int h = (unsigned int)len * 2166136261u;
    for (size_t i = 0; i < len; ++i) {
        h ^= (unsigned char)text[i];
        h *= 16777619u;
    }
    return h;
}

static unsigned int clamp_cache_slot(const TTF_Font* font, const char* text, int max_width) {
    unsigned long long k = (unsigned long long)(uintptr_t)font;
    k ^= (unsigned long long)(uintptr_t)text;
    k ^= (unsigned long long)(unsigned int)max_width * 11400714819323198485ull;
    return (unsigned int)(k % CLAMP_CACHE_SIZE);
}

static void current_text_scale(float* sx, float* sy) {
    RenderContext* ctx=getRenderContext();
    ide_text_pixel_scale(ctx?(SDL_Renderer*)ctx->renderer:NULL,sx,sy);
}

/* Prefix and full-text measurements use the same raster font as painting.
   Integer logical callers receive enclosing bounds, within one logical pixel. */
static int measured_width(const char* text, TTF_Font* font, bool utf8) {
    if (!text || !text[0] || !font) return 0;
    float sx,sy,ratio; current_text_scale(&sx,&sy);
    float scale=fmaxf(1,fmaxf(sx,sy));
    int w=0,h=0;
    if (utf8 && measureNativeFallbackText(font,text,&w,&h))
        return (int)ceilf(ceilf(w*scale)/sx);
    TTF_Font* raster=getRasterizedFontForScale(font,scale,&ratio);
    if (!raster) raster=font;
    int result=utf8?TTF_SizeUTF8(raster,text,&w,&h):TTF_SizeText(raster,text,&w,&h);
    return result==0?(int)ceilf(w/sx):0;
}

int getTextWidthWithFont(const char* text, TTF_Font* font) {
    if (!text || text[0] == '\0') return 0;
    if (!font) return 0;

    return measured_width(text,font,false);
}

int getTextWidth(const char* text) {
    return getTextWidthWithFont(text, getActiveFont());
}

int getTextWidthNWithFont(const char* text, int n, TTF_Font* font) {
    if (!text || n <= 0) return 0;
    if (!font) return 0;

    size_t len = strnlen(text, (size_t)n);
    if (len == 0) return 0;

    char temp[1024];
    char* text_buf = temp;
    if (len >= sizeof(temp)) {
        text_buf = (char*)malloc(len + 1);
        if (!text_buf) return 0;
    }

    memcpy(text_buf, text, len);
    text_buf[len] = '\0';

    int w = measured_width(text_buf,font,false);
    if (text_buf != temp) free(text_buf);

    return w;
}

int getTextWidthN(const char* text, int n) {
    return getTextWidthNWithFont(text, n, getActiveFont());
}

int getTextWidthUTF8WithFont(const char* text, TTF_Font* font) {
    if (!text || text[0] == '\0') return 0;
    if (!font) return 0;

    return measured_width(text,font,true);
}

size_t getTextClampedLength(const char* text, int maxWidth) {
    return getTextClampedLengthWithFont(text, maxWidth, getActiveFont());
}

size_t getTextClampedLengthWithFont(const char* text, int maxWidth, TTF_Font* font) {
    if (!text) return 0;
    if (maxWidth <= 0) return strlen(text);
    if (!font) return strlen(text);

    size_t len = strlen(text);
    if (len == 0) return 0;
    float sx,sy;current_text_scale(&sx,&sy);
    uint64_t revision=getFontSystemRevision();
    unsigned int fp = clamp_cache_fingerprint(text, len);
    unsigned int slot = clamp_cache_slot(font, text, maxWidth);
    ClampCacheEntry* entry = &s_clamp_cache[slot];
    if (entry->valid &&
        entry->font == font &&
        entry->font_revision == revision && entry->scale_x == sx && entry->scale_y == sy &&
        entry->text_ptr == text &&
        entry->max_width == maxWidth &&
        entry->text_len == len &&
        entry->text_fingerprint == fp) {
        return entry->cutoff;
    }

    size_t cutoff = 0;

    if (getTextWidthNWithFont(text, (int)len, font) <= maxWidth) {
        cutoff = len;
    } else {
        size_t low = 0;
        size_t high = len;
        while (low < high) {
            size_t mid = low + ((high - low + 1) / 2);
            int width = getTextWidthNWithFont(text, (int)mid, font);
            if (width <= maxWidth) {
                low = mid;
            } else {
                high = mid - 1;
            }
        }
        cutoff = low;
    }

    entry->font = font;
    entry->font_revision=revision;entry->scale_x=sx;entry->scale_y=sy;
    entry->text_ptr = text;
    entry->max_width = maxWidth;
    entry->text_len = len;
    entry->text_fingerprint = fp;
    entry->cutoff = cutoff;
    entry->valid = 1;
    return cutoff;
}
