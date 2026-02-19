#include "engine/Render/render_helpers.h"
#include "engine/Render/render_text_helpers.h"
#include "engine/Render/render_font.h"

static void verify_terminal_text_apis(void) {
    TTF_Font* terminal_font = getTerminalFont();
    TTF_Font* ui_font = getActiveFont();

    (void)getTextWidthWithFont("abc", terminal_font);
    (void)getTextWidthNWithFont("abcdef", 3, ui_font);
    (void)getTextWidthUTF8WithFont("hello ✓", terminal_font);

    SDL_Color color = {255, 255, 255, 255};
    drawTextUTF8WithFontColor(0, 0, "terminal", terminal_font, color, false);
}

static void (*volatile terminal_text_api_check_ref)(void)
#if defined(__clang__) || defined(__GNUC__)
    __attribute__((unused))
#endif
    = verify_terminal_text_apis;
