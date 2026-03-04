#ifndef IDE_UI_PANEL_TEXT_FIELD_H
#define IDE_UI_PANEL_TEXT_FIELD_H

#include "engine/Render/render_helpers.h"
#include "engine/Render/render_text_helpers.h"
#include "engine/Render/render_font.h"
#include "ide/UI/shared_theme_font_adapter.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <string.h>

typedef struct UIPanelTextFieldSpec {
    SDL_Rect rect;
    const char* text;
    const char* placeholder;
    bool focused;
    int cursor;
    CoreFontTextSizeTier tier;
} UIPanelTextFieldSpec;

static inline void ui_panel_text_field_render(SDL_Renderer* renderer,
                                              const UIPanelTextFieldSpec* spec) {
    if (!renderer || !spec) return;

    IDEThemePalette palette = {0};
    (void)ide_shared_theme_resolve_palette(&palette);

    SDL_SetRenderDrawColor(renderer,
                           palette.input_fill.r,
                           palette.input_fill.g,
                           palette.input_fill.b,
                           80);
    SDL_RenderFillRect(renderer, &spec->rect);

    SDL_Color border = spec->focused ? palette.input_focus_border : palette.input_border;
    SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, 255);
    SDL_RenderDrawRect(renderer, &spec->rect);

    TTF_Font* font = getUIFontByTier(spec->tier);
    if (!font) font = getActiveFont();

    const int textInsetX = 6;
    const int textInsetY = 3;
    SDL_Rect clipRect = {
        spec->rect.x + 4,
        spec->rect.y + 1,
        spec->rect.w - 8,
        spec->rect.h - 2
    };
    if (clipRect.w < 0) clipRect.w = 0;
    if (clipRect.h < 0) clipRect.h = 0;

    const char* text = spec->text ? spec->text : "";
    if (text[0]) {
        drawTextUTF8WithFontColorClipped(spec->rect.x + textInsetX,
                                         spec->rect.y + textInsetY,
                                         text,
                                         font,
                                         palette.text_primary,
                                         false,
                                         &clipRect);
    } else if (!spec->focused && spec->placeholder && spec->placeholder[0]) {
        drawTextUTF8WithFontColorClipped(spec->rect.x + textInsetX,
                                         spec->rect.y + textInsetY,
                                         spec->placeholder,
                                         font,
                                         palette.text_muted,
                                         false,
                                         &clipRect);
    }

    if (spec->focused) {
        int cursor = spec->cursor;
        if (cursor < 0) cursor = 0;
        size_t textLen = strlen(text);
        if ((size_t)cursor > textLen) cursor = (int)textLen;
        int caretAdvance = (text[0] && cursor > 0) ? getTextWidthNWithFont(text, cursor, font) : 0;
        int caretX = spec->rect.x + textInsetX + caretAdvance;
        int caretMinX = spec->rect.x + 4;
        int caretMaxX = spec->rect.x + spec->rect.w - 4;
        if (caretX < caretMinX) caretX = caretMinX;
        if (caretX > caretMaxX) caretX = caretMaxX;
        SDL_Rect caretClip = {
            spec->rect.x + 4,
            spec->rect.y + 2,
            spec->rect.w - 8,
            spec->rect.h - 4
        };
        if (caretClip.w > 0 && caretClip.h > 0) {
            pushClipRect(&caretClip);
            SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, 255);
            SDL_RenderDrawLine(renderer,
                               caretX,
                               spec->rect.y + textInsetY,
                               caretX,
                               spec->rect.y + spec->rect.h - 4);
            popClipRect();
        }
    }
}

#endif
