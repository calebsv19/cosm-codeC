#ifndef IDE_UI_BUTTON_H
#define IDE_UI_BUTTON_H

#include "ide/UI/shared_theme_font_adapter.h"
#include "kit_ui.h"

#include <SDL2/SDL.h>
#include <stdbool.h>

typedef enum IDEUIButtonVariant {
    IDE_UI_BUTTON_VARIANT_DEFAULT = 0,
    IDE_UI_BUTTON_VARIANT_PRIMARY,
    IDE_UI_BUTTON_VARIANT_POSITIVE
} IDEUIButtonVariant;

typedef struct IDEUIButtonState {
    bool hovered;
    bool selected;
    bool pressed;
    bool disabled;
    bool focused;
} IDEUIButtonState;

typedef struct IDEUIButtonResolvedStyle {
    SDL_Color fill;
    SDL_Color outline;
    SDL_Color text;
} IDEUIButtonResolvedStyle;

void ide_ui_button_state_init(IDEUIButtonState* state);
bool ide_ui_button_resolve_palette(IDEThemePalette* out_palette);
KitUiButtonVariant ide_ui_button_kit_variant(IDEUIButtonVariant variant);
bool ide_ui_button_make_spec(KitUiButtonSpec* out_spec,
                             const char* label,
                             IDEUIButtonVariant variant,
                             IDEUIButtonState state);
bool ide_ui_button_theme_from_palette(const IDEThemePalette* palette,
                                      KitUiButtonTheme* out_theme);
bool ide_ui_button_resolve_style(const IDEThemePalette* palette,
                                 const char* label,
                                 IDEUIButtonVariant variant,
                                 IDEUIButtonState state,
                                 IDEUIButtonResolvedStyle* out_style);
bool ide_ui_button_compact_appearance(KitUiButtonAppearance* out_appearance,
                                      KitUiButtonLayout* out_layout);
SDL_Rect ide_ui_button_outer_focus_rect(SDL_Rect rect);

#endif
