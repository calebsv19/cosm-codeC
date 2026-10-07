#ifndef IDE_UI_PANEL_TEXT_FIELD_H
#define IDE_UI_PANEL_TEXT_FIELD_H

#include "engine/Render/render_helpers.h"
#include "engine/Render/render_text_helpers.h"
#include "engine/Render/render_font.h"
#include "ide/UI/shared_theme_font_adapter.h"
#include "kit_ui_text_edit.h"
#include "panel_button_scope.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <string.h>

typedef struct UIPanelTextFieldSpec {
    SDL_Rect rect;
    UIPane* owner_pane;
    IDEPanelButtonGroup owner_group;
    const char* text;
    const char* placeholder;
    bool focused;
    bool hide_caret; /* Blink visibility is separate from viewport focus/reveal. */
    int cursor;
    const KitUiTextEdit* edit;
    CoreFontTextSizeTier tier;
} UIPanelTextFieldSpec;

void ui_panel_text_field_render(SDL_Renderer* renderer, const UIPanelTextFieldSpec* spec);

#endif
