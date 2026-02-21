#include "ide/UI/shared_theme_font_adapter.h"

#include <stdio.h>
#include <stdlib.h>

static void fail(const char *msg) {
    fprintf(stderr, "shared_theme_font_adapter_test: %s\n", msg);
    exit(1);
}

static void expect(int cond, const char *msg) {
    if (!cond) {
        fail(msg);
    }
}

int main(void) {
    UITheme theme = {
        .bgMenuBar = {40, 40, 40, 255},
        .bgEditor = {20, 20, 20, 255},
        .bgIconBar = {40, 40, 40, 255},
        .bgToolBar = {30, 30, 30, 255},
        .bgControlPanel = {30, 30, 30, 255},
        .bgTerminal = {25, 25, 25, 255},
        .bgPopup = {50, 50, 50, 255},
        .border = {255, 255, 255, 255},
        .text = {255, 255, 255, 255},
    };
    char path[256] = {0};
    int point_size = 0;
    SDL_Color clear = {0};
    size_t i = 0;
    const char* theme_presets[] = {
        "ide_gray",
        "daw_default",
        "dark_default",
        "light_default"
    };

    unsetenv("IDE_USE_SHARED_THEME_FONT");
    unsetenv("IDE_USE_SHARED_THEME");
    unsetenv("IDE_USE_SHARED_FONT");
    unsetenv("IDE_THEME_PRESET");
    unsetenv("IDE_FONT_PRESET");

    setenv("IDE_USE_SHARED_THEME_FONT", "0", 1);
    expect(!ide_shared_theme_apply(&theme), "theme should remain fallback when shared mode is disabled");
    setenv("IDE_USE_SHARED_THEME_FONT", "1", 1);

    setenv("IDE_USE_SHARED_FONT", "0", 1);
    expect(!ide_shared_font_resolve_role(CORE_FONT_ROLE_UI_REGULAR,
                                         CORE_FONT_TEXT_SIZE_HEADER,
                                         path,
                                         sizeof(path),
                                         &point_size),
           "font should remain fallback when shared mode is disabled");
    setenv("IDE_USE_SHARED_FONT", "1", 1);

    setenv("IDE_THEME_PRESET", "ide_gray", 1);
    setenv("IDE_FONT_PRESET", "ide", 1);

    expect(ide_shared_theme_apply(&theme), "theme should resolve in shared mode");
    expect(theme.bgEditor.r == 20 && theme.bgEditor.g == 20 && theme.bgEditor.b == 20,
           "editor bg should map to ide_gray surface_0");

    clear = ide_shared_theme_background_color();
    expect(clear.r == 20 && clear.g == 20 && clear.b == 20, "clear color should come from shared theme");

    expect(ide_shared_font_resolve_role(CORE_FONT_ROLE_UI_REGULAR,
                                        CORE_FONT_TEXT_SIZE_HEADER,
                                        path,
                                        sizeof(path),
                                        &point_size),
           "ui_regular header size should resolve");
    expect(point_size > 12, "header size should be larger than base");

    expect(ide_shared_font_resolve_role(CORE_FONT_ROLE_UI_REGULAR,
                                        CORE_FONT_TEXT_SIZE_CAPTION,
                                        path,
                                        sizeof(path),
                                        &point_size),
           "ui_regular caption size should resolve");
    expect(point_size < 12, "caption size should be smaller than base");

    for (i = 0; i < sizeof(theme_presets) / sizeof(theme_presets[0]); ++i) {
        setenv("IDE_THEME_PRESET", theme_presets[i], 1);
        expect(ide_shared_theme_apply(&theme), "theme preset matrix should resolve");
    }

    puts("shared_theme_font_adapter_test: success");
    return 0;
}
