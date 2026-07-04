#include "ide/UI/ide_ui_button.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void fail(const char* msg) {
    fprintf(stderr, "ide_ui_button_adapter_test: %s\n", msg);
    exit(1);
}

static void expect(int condition, const char* msg) {
    if (!condition) {
        fail(msg);
    }
}

static int color_equal(SDL_Color a, SDL_Color b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

int main(void) {
    IDEThemePalette palette = {
        .button_fill = {40, 44, 50, 255},
        .button_fill_active = {70, 78, 92, 255},
        .button_border = {110, 118, 130, 255},
        .selection_fill = {82, 120, 190, 255},
        .accent_primary = {92, 150, 255, 255},
        .text_primary = {232, 235, 238, 255},
        .text_muted = {150, 155, 160, 255},
    };
    IDEUIButtonState state;
    IDEUIButtonResolvedStyle idle;
    IDEUIButtonResolvedStyle selected;
    IDEUIButtonResolvedStyle selected_hovered;
    IDEUIButtonResolvedStyle pressed;
    IDEUIButtonResolvedStyle disabled;
    IDEThemePalette resolved_palette;
    KitUiButtonSpec spec;
    KitUiButtonAppearance appearance;
    KitUiButtonLayout layout;
    SDL_Rect focus_rect;

    ide_ui_button_state_init(&state);
    expect(ide_ui_button_resolve_palette(&resolved_palette),
           "IDE button palette should resolve");
    expect(resolved_palette.button_fill.a == 255,
           "resolved palette should provide opaque button fill");
    expect(resolved_palette.text_primary.a == 255,
           "resolved palette should provide opaque primary text");

    expect(ide_ui_button_resolve_style(&palette,
                                       "Run",
                                       IDE_UI_BUTTON_VARIANT_DEFAULT,
                                       state,
                                       &idle),
           "idle style should resolve");
    expect(color_equal(idle.fill, palette.button_fill), "idle fill should follow IDE palette");
    expect(color_equal(idle.outline, palette.button_border), "idle outline should follow IDE palette");
    expect(color_equal(idle.text, palette.text_primary), "idle text should follow IDE palette");

    state.selected = true;
    expect(ide_ui_button_make_spec(&spec,
                                   "Run",
                                   IDE_UI_BUTTON_VARIANT_PRIMARY,
                                   state),
           "kit spec should populate");
    expect(strcmp(spec.label, "Run") == 0, "kit spec label should be borrowed");
    expect(spec.variant == KIT_UI_BUTTON_VARIANT_PRIMARY, "primary variant should map to KitUI");
    expect(spec.state.selected == 1, "selected state should map to KitUI");

    expect(ide_ui_button_resolve_style(&palette,
                                       "Run",
                                       IDE_UI_BUTTON_VARIANT_DEFAULT,
                                       state,
                                       &selected),
           "selected style should resolve");
    expect(!color_equal(selected.fill, idle.fill), "selected fill should differ from idle");

    state.hovered = true;
    expect(ide_ui_button_resolve_style(&palette,
                                       "Run",
                                       IDE_UI_BUTTON_VARIANT_DEFAULT,
                                       state,
                                       &selected_hovered),
           "selected hover style should resolve");
    expect(!color_equal(selected_hovered.fill, selected.fill),
           "selected hover fill should differ from selected fill");
    expect(color_equal(selected_hovered.outline, palette.accent_primary),
           "selected hover outline should use highlight");

    state.pressed = true;
    expect(ide_ui_button_resolve_style(&palette,
                                       "Run",
                                       IDE_UI_BUTTON_VARIANT_DEFAULT,
                                       state,
                                       &pressed),
           "pressed style should resolve");
    expect(!color_equal(pressed.fill, selected_hovered.fill),
           "pressed fill should differ from selected hover fill");

    ide_ui_button_state_init(&state);
    state.disabled = true;
    expect(ide_ui_button_resolve_style(&palette,
                                       "Run",
                                       IDE_UI_BUTTON_VARIANT_DEFAULT,
                                       state,
                                       &disabled),
           "disabled style should resolve");
    expect(color_equal(disabled.text, palette.text_muted),
           "disabled text should use muted color");

    expect(ide_ui_button_compact_appearance(&appearance, &layout),
           "compact appearance should resolve");
    expect(appearance.border_thickness == 1.0f, "compact border should stay one pixel");
    expect(layout.text_offset_x == 4.0f, "compact text offset should follow padding");

    focus_rect = ide_ui_button_outer_focus_rect((SDL_Rect){10, 20, 30, 40});
    expect(focus_rect.x == 9 && focus_rect.y == 19 &&
           focus_rect.w == 32 && focus_rect.h == 42,
           "focus rect should expand by one pixel");

    puts("ide_ui_button_adapter_test: success");
    return 0;
}
