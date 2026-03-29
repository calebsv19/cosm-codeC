#include "ide/UI/shared_theme_font_adapter.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
        "studio_blue",
        "harbor_blue",
        "midnight_contrast",
        "soft_light",
        "standard_grey",
        "greyscale"
    };

    unsetenv("IDE_USE_SHARED_THEME_FONT");
    unsetenv("IDE_USE_SHARED_THEME");
    unsetenv("IDE_USE_SHARED_FONT");
    unsetenv("IDE_THEME_PRESET");
    unsetenv("IDE_FONT_PRESET");
    unsetenv("IDE_FONT_ZOOM_STEP");

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

    setenv("IDE_THEME_PRESET", "standard_grey", 1);
    setenv("IDE_FONT_PRESET", "ide", 1);

    expect(ide_shared_theme_apply(&theme), "theme should resolve in shared mode");
    expect(theme.bgEditor.r == 20 && theme.bgEditor.g == 20 && theme.bgEditor.b == 20,
           "editor bg should map to standard_grey surface_0");

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

    {
        int baseline_point = point_size;
        expect(ide_shared_font_set_zoom_step(2),
               "setting positive zoom step should report change");
        expect(ide_shared_font_zoom_step() == 2,
               "zoom step getter should return updated value");
        expect(ide_shared_font_resolve_role(CORE_FONT_ROLE_UI_REGULAR,
                                            CORE_FONT_TEXT_SIZE_CAPTION,
                                            path,
                                            sizeof(path),
                                            &point_size),
               "caption size should resolve with zoom");
        expect(point_size > baseline_point,
               "positive zoom should increase resolved point size");
        expect(ide_shared_font_step_by(-3),
               "decreasing zoom step should report change");
        expect(ide_shared_font_zoom_step() == -1,
               "zoom step decrement should apply");
        expect(ide_shared_font_reset_zoom_step(),
               "zoom reset should report change");
        expect(ide_shared_font_zoom_step() == 0,
               "zoom reset should return to zero");
    }

    expect(ide_shared_theme_set_preset("midnight_contrast"),
           "runtime preset set should accept known preset");
    expect(ide_shared_theme_current_preset(path, sizeof(path)),
           "runtime preset query should succeed");
    expect(strcmp(path, "midnight_contrast") == 0,
           "runtime preset query should return the set preset");
    expect(ide_shared_theme_cycle_next(), "runtime next-cycle should succeed");
    expect(ide_shared_theme_current_preset(path, sizeof(path)),
           "runtime preset query should succeed after cycling next");
    expect(strcmp(path, "soft_light") == 0,
           "runtime next-cycle should move to soft_light");
    expect(ide_shared_theme_cycle_prev(), "runtime prev-cycle should succeed");
    expect(ide_shared_theme_current_preset(path, sizeof(path)),
           "runtime preset query should succeed after cycling prev");
    expect(strcmp(path, "midnight_contrast") == 0,
           "runtime prev-cycle should move back to midnight_contrast");
    expect(!ide_shared_theme_set_preset("missing_preset"),
           "runtime preset set should reject unknown preset");

    for (i = 0; i < sizeof(theme_presets) / sizeof(theme_presets[0]); ++i) {
        setenv("IDE_THEME_PRESET", theme_presets[i], 1);
        expect(ide_shared_theme_apply(&theme), "theme preset matrix should resolve");
    }

    puts("shared_theme_font_adapter_test: success");
    return 0;
}
