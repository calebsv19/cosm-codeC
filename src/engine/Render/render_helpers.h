#ifndef RENDER_HELPERS_H
#define RENDER_HELPERS_H

#include "engine/Render/renderer_backend.h"
#include "core/TextSelection/text_selection_manager.h"
#include "core_font.h"
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>

struct UIPane;

typedef struct SelectableTextOptions {
    struct UIPane* pane;
    void* owner;
    UIPaneRole owner_role;
    int x;
    int y;
    int maxWidth;
    const char* text;
    unsigned int flags;
} SelectableTextOptions;

// Draw text with default styling
void drawText(int x, int y, const char* text);
void drawTextWithFont(int x, int y, const char* text, TTF_Font* font);
void drawTextWithTier(int x, int y, const char* text, CoreFontTextSizeTier tier);
void drawTextUTF8WithFontColor(int x, int y, const char* text, TTF_Font* font,
                               SDL_Color color, bool bold);
void drawTextUTF8WithFontColorClipped(int x, int y, const char* text, TTF_Font* font,
                                      SDL_Color color, bool bold, const SDL_Rect* clipRect);

// Draw clipped text within a max width
void drawClippedText(int x, int y, const char* text, int maxWidth);

void drawSelectableText(const SelectableTextOptions* options);

// UI widgets
void renderButton(struct UIPane* pane, SDL_Rect rect, const char* label);

void pushClipRect(const SDL_Rect* rect);
void popClipRect(void);
void render_text_cache_shutdown(void);

#endif
