#include "ide/UI/ide_ui_button.h"

#include <string.h>

static CoreThemeColor ide_ui_button_core_color(SDL_Color color) {
    return (CoreThemeColor){ color.r, color.g, color.b, color.a };
}

static SDL_Color ide_ui_button_sdl_color(CoreThemeColor color) {
    return (SDL_Color){ color.r, color.g, color.b, color.a };
}

void ide_ui_button_state_init(IDEUIButtonState* state) {
    if (!state) return;
    memset(state, 0, sizeof(*state));
}

bool ide_ui_button_resolve_palette(IDEThemePalette* out_palette) {
    if (!out_palette) return false;
    if (ide_shared_theme_resolve_palette(out_palette)) {
        return true;
    }

    memset(out_palette, 0, sizeof(*out_palette));
    out_palette->button_fill = (SDL_Color){80, 80, 80, 255};
    out_palette->button_fill_active = (SDL_Color){120, 120, 120, 255};
    out_palette->button_border = (SDL_Color){180, 180, 180, 255};
    out_palette->selection_fill = (SDL_Color){82, 120, 190, 255};
    out_palette->accent_primary = (SDL_Color){140, 150, 240, 255};
    out_palette->text_primary = (SDL_Color){230, 230, 230, 255};
    out_palette->text_muted = (SDL_Color){160, 160, 160, 255};
    return true;
}

KitUiButtonVariant ide_ui_button_kit_variant(IDEUIButtonVariant variant) {
    switch (variant) {
        case IDE_UI_BUTTON_VARIANT_PRIMARY:
            return KIT_UI_BUTTON_VARIANT_PRIMARY;
        case IDE_UI_BUTTON_VARIANT_POSITIVE:
            return KIT_UI_BUTTON_VARIANT_POSITIVE;
        case IDE_UI_BUTTON_VARIANT_DEFAULT:
        default:
            return KIT_UI_BUTTON_VARIANT_DEFAULT;
    }
}

bool ide_ui_button_make_spec(KitUiButtonSpec* out_spec,
                             const char* label,
                             IDEUIButtonVariant variant,
                             IDEUIButtonState state) {
    if (!out_spec) return false;

    kit_ui_button_spec_init(out_spec, label ? label : "");
    out_spec->variant = ide_ui_button_kit_variant(variant);
    out_spec->state.hovered = state.hovered ? 1 : 0;
    out_spec->state.selected = state.selected ? 1 : 0;
    out_spec->state.pressed = state.pressed ? 1 : 0;
    out_spec->state.disabled = state.disabled ? 1 : 0;
    out_spec->state.focused = state.focused ? 1 : 0;
    return true;
}

bool ide_ui_button_theme_from_palette(const IDEThemePalette* palette,
                                      KitUiButtonTheme* out_theme) {
    if (!palette || !out_theme) return false;

    out_theme->idle_fill = ide_ui_button_core_color(palette->button_fill);
    out_theme->selected_fill = ide_ui_button_core_color(palette->button_fill_active);
    out_theme->hover_fill = ide_ui_button_core_color(palette->selection_fill);
    out_theme->positive_fill = ide_ui_button_core_color(palette->accent_primary);
    out_theme->outline_idle = ide_ui_button_core_color(palette->button_border);
    out_theme->outline_highlight = ide_ui_button_core_color(palette->accent_primary);
    out_theme->text_primary = ide_ui_button_core_color(palette->text_primary);
    out_theme->text_muted = ide_ui_button_core_color(palette->text_muted);
    return true;
}

bool ide_ui_button_resolve_style(const IDEThemePalette* palette,
                                 const char* label,
                                 IDEUIButtonVariant variant,
                                 IDEUIButtonState state,
                                 IDEUIButtonResolvedStyle* out_style) {
    KitUiButtonTheme theme;
    KitUiButtonSpec spec;
    KitUiButtonStyle style;

    if (!out_style ||
        !ide_ui_button_theme_from_palette(palette, &theme) ||
        !ide_ui_button_make_spec(&spec, label, variant, state) ||
        !kit_ui_button_style_resolve(&theme, &spec, &style)) {
        return false;
    }

    out_style->fill = ide_ui_button_sdl_color(style.fill);
    out_style->outline = ide_ui_button_sdl_color(style.outline);
    out_style->text = ide_ui_button_sdl_color(style.text);
    return true;
}

bool ide_ui_button_compact_appearance(KitUiButtonAppearance* out_appearance,
                                      KitUiButtonLayout* out_layout) {
    if (!out_appearance || !out_layout) return false;
    if (!kit_ui_button_appearance_preset(KIT_UI_BUTTON_APPEARANCE_DEFAULT,
                                         out_appearance)) {
        return false;
    }

    out_appearance->corner_radius = 0.0f;
    out_appearance->border_thickness = 1.0f;
    out_appearance->padding_x = 4.0f;
    out_appearance->padding_y = 2.0f;
    kit_ui_button_layout_from_appearance(out_appearance, out_layout);
    return true;
}

SDL_Rect ide_ui_button_outer_focus_rect(SDL_Rect rect) {
    rect.x -= 1;
    rect.y -= 1;
    rect.w += 2;
    rect.h += 2;
    return rect;
}
