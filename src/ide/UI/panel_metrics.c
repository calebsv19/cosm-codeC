#include "ide/UI/panel_metrics.h"

#include "engine/Render/render_font.h"

#include <SDL2/SDL_ttf.h>

static int caption_font_height_px(void) {
    TTF_Font* font = getUIFontByTier(CORE_FONT_TEXT_SIZE_CAPTION);
    if (!font) {
        font = getActiveFont();
    }
    if (!font) {
        return 14;
    }

    {
        int h = TTF_FontHeight(font);
        if (h > 0) {
            return h;
        }
    }
    return 14;
}

int ide_ui_dense_row_height(void) {
    int h = caption_font_height_px() + 2;
    if (h < 14) h = 14;
    return h;
}

int ide_ui_dense_header_height(void) {
    int h = caption_font_height_px() + 3;
    if (h < 14) h = 14;
    return h;
}

int ide_ui_tree_indent_width(void) {
    int row = ide_ui_dense_row_height();
    int indent = (row * 10 + 7) / 14;
    if (indent < 10) indent = 10;
    return indent;
}
