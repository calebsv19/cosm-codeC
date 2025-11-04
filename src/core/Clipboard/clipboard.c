#include "core/Clipboard/clipboard.h"

#include <SDL2/SDL.h>
#include <stdlib.h>
#include <string.h>

static char* s_cached_text = NULL;

static void clipboard_cache_store(const char* text) {
    if (s_cached_text) {
        free(s_cached_text);
        s_cached_text = NULL;
    }
    if (text && text[0] != '\0') {
        size_t len = strlen(text);
        s_cached_text = (char*)malloc(len + 1);
        if (!s_cached_text) return;
        memcpy(s_cached_text, text, len);
        s_cached_text[len] = '\0';
    }
}

bool clipboard_copy_text(const char* text) {
    if (!text) text = "";

    bool copied = (SDL_SetClipboardText(text) == 0);
    if (!copied) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "[Clipboard] SDL_SetClipboardText failed: %s",
                    SDL_GetError());
    }

    clipboard_cache_store(text);
    return copied;
}

char* clipboard_paste_text(void) {
    char* sdl_text = SDL_GetClipboardText();
    if (!sdl_text) {
        const char* cached = clipboard_peek_cached();
        if (!cached) return NULL;
        size_t len = strlen(cached);
        char* copy = (char*)malloc(len + 1);
        if (!copy) return NULL;
        memcpy(copy, cached, len);
        copy[len] = '\0';
        return copy;
    }

    if (sdl_text[0] == '\0') {
        SDL_free(sdl_text);
        const char* cached = clipboard_peek_cached();
        if (!cached) return NULL;
        size_t len = strlen(cached);
        char* copy = (char*)malloc(len + 1);
        if (!copy) return NULL;
        memcpy(copy, cached, len);
        copy[len] = '\0';
        return copy;
    }

    size_t len = strlen(sdl_text);
    char* copy = (char*)malloc(len + 1);
    if (copy) {
        memcpy(copy, sdl_text, len);
        copy[len] = '\0';
    }
    SDL_free(sdl_text);
    return copy;
}

void clipboard_free_text(char* text) {
    if (text) {
        free(text);
    }
}

const char* clipboard_peek_cached(void) {
    return s_cached_text;
}
