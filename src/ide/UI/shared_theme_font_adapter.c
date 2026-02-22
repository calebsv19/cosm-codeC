#include "ide/UI/shared_theme_font_adapter.h"

#include "core_theme.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static bool parse_bool_env(const char *value, bool *out_value) {
    char lowered[16];
    size_t i = 0;
    if (!value || !value[0] || !out_value) {
        return false;
    }
    for (; value[i] && i < sizeof(lowered) - 1; ++i) {
        lowered[i] = (char)tolower((unsigned char)value[i]);
    }
    lowered[i] = '\0';

    if (strcmp(lowered, "1") == 0 || strcmp(lowered, "true") == 0 || strcmp(lowered, "yes") == 0 ||
        strcmp(lowered, "on") == 0) {
        *out_value = true;
        return true;
    }
    if (strcmp(lowered, "0") == 0 || strcmp(lowered, "false") == 0 || strcmp(lowered, "no") == 0 ||
        strcmp(lowered, "off") == 0) {
        *out_value = false;
        return true;
    }
    return false;
}

static bool is_shared_toggle_enabled(const char *override_var_name) {
    const char *override = getenv(override_var_name);
    bool out_value = false;
    if (parse_bool_env(override, &out_value)) {
        return out_value;
    }
    if (parse_bool_env(getenv("IDE_USE_SHARED_THEME_FONT"), &out_value)) {
        return out_value;
    }
    return true;
}

static SDL_Color theme_color_or_default(const CoreThemePreset *preset,
                                        CoreThemeColorToken token,
                                        SDL_Color fallback) {
    CoreThemeColor raw = {0};
    CoreResult r = core_theme_get_color(preset, token, &raw);
    if (r.code != CORE_OK) {
        return fallback;
    }
    return (SDL_Color){raw.r, raw.g, raw.b, raw.a};
}

static Uint8 clamp_channel(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (Uint8)v;
}

static SDL_Color brighten(SDL_Color in, int amount) {
    return (SDL_Color){
        clamp_channel((int)in.r + amount),
        clamp_channel((int)in.g + amount),
        clamp_channel((int)in.b + amount),
        in.a
    };
}

static SDL_Color darken(SDL_Color in, int amount) {
    return (SDL_Color){
        clamp_channel((int)in.r - amount),
        clamp_channel((int)in.g - amount),
        clamp_channel((int)in.b - amount),
        in.a
    };
}

static int stat_path_exists(const char *path, void *user) {
    struct stat st;
    (void)user;
    return path && stat(path, &st) == 0;
}

bool ide_shared_theme_apply(UITheme *theme) {
    const char *preset_name;
    CoreThemePreset preset = {0};
    CoreResult r;

    if (!theme || !is_shared_toggle_enabled("IDE_USE_SHARED_THEME")) {
        return false;
    }

    preset_name = getenv("IDE_THEME_PRESET");
    if (!preset_name || !preset_name[0]) {
        preset_name = "ide_gray";
    }

    r = core_theme_get_preset_by_name(preset_name, &preset);
    if (r.code != CORE_OK) {
        r = core_theme_get_preset(CORE_THEME_PRESET_IDE_GRAY, &preset);
        if (r.code != CORE_OK) {
            return false;
        }
    }

    {
        SDL_Color menu_bar_base =
            theme_color_or_default(&preset, CORE_THEME_COLOR_SURFACE_1, (SDL_Color){40, 40, 40, 255});
        SDL_Color swapped_bar_bg = darken(menu_bar_base, 10);
        theme->bgMenuBar = swapped_bar_bg;
        theme->bgIconBar = swapped_bar_bg;
    }
    theme->bgEditor =
        theme_color_or_default(&preset, CORE_THEME_COLOR_SURFACE_0, (SDL_Color){20, 20, 20, 255});
    theme->bgToolBar =
        theme_color_or_default(&preset, CORE_THEME_COLOR_SURFACE_1, (SDL_Color){30, 30, 30, 255});
    theme->bgControlPanel =
        theme_color_or_default(&preset, CORE_THEME_COLOR_SURFACE_1, (SDL_Color){30, 30, 30, 255});
    theme->bgTerminal =
        theme_color_or_default(&preset, CORE_THEME_COLOR_SURFACE_0, (SDL_Color){25, 25, 25, 255});
    theme->bgPopup =
        theme_color_or_default(&preset, CORE_THEME_COLOR_SURFACE_2, (SDL_Color){50, 50, 50, 255});
    theme->border =
        theme_color_or_default(&preset, CORE_THEME_COLOR_ACCENT_PRIMARY, (SDL_Color){255, 255, 255, 255});
    theme->text =
        theme_color_or_default(&preset, CORE_THEME_COLOR_TEXT_PRIMARY, (SDL_Color){255, 255, 255, 255});

    return true;
}

SDL_Color ide_shared_theme_background_color(void) {
    UITheme themed = {
        .bgMenuBar = {40, 40, 40, 255},
        .bgEditor = {30, 30, 30, 255},
        .bgIconBar = {40, 40, 40, 255},
        .bgToolBar = {30, 30, 30, 255},
        .bgControlPanel = {30, 30, 30, 255},
        .bgTerminal = {25, 25, 25, 255},
        .bgPopup = {50, 50, 50, 255},
        .border = {255, 255, 255, 255},
        .text = {255, 255, 255, 255},
    };
    if (ide_shared_theme_apply(&themed)) {
        return themed.bgEditor;
    }
    return (SDL_Color){30, 30, 30, 255};
}

void ide_shared_theme_button_colors(SDL_Color *out_fill,
                                    SDL_Color *out_fill_active,
                                    SDL_Color *out_border,
                                    SDL_Color *out_text) {
    UITheme themed = {
        .bgMenuBar = {40, 40, 40, 255},
        .bgEditor = {30, 30, 30, 255},
        .bgIconBar = {40, 40, 40, 255},
        .bgToolBar = {30, 30, 30, 255},
        .bgControlPanel = {30, 30, 30, 255},
        .bgTerminal = {25, 25, 25, 255},
        .bgPopup = {50, 50, 50, 255},
        .border = {255, 255, 255, 255},
        .text = {255, 255, 255, 255},
    };
    if (ide_shared_theme_apply(&themed)) {
        SDL_Color base = brighten(themed.bgMenuBar, 10);
        if (out_fill) {
            *out_fill = base;
        }
        if (out_fill_active) {
            *out_fill_active = brighten(base, 18);
        }
        if (out_border) {
            *out_border = themed.border;
        }
        if (out_text) {
            *out_text = themed.text;
        }
        return;
    }

    if (out_fill) {
        *out_fill = (SDL_Color){100, 100, 100, 255};
    }
    if (out_fill_active) {
        *out_fill_active = (SDL_Color){140, 140, 140, 255};
    }
    if (out_border) {
        *out_border = (SDL_Color){255, 255, 255, 255};
    }
    if (out_text) {
        *out_text = (SDL_Color){255, 255, 255, 255};
    }
}

SDL_Color ide_shared_theme_pane_hover_border_color(void) {
    // Keep pane hover visibly blue-tinted (not white) for clear feedback.
    return (SDL_Color){140, 150, 240, 255};
}

void ide_shared_theme_editor_border_colors(SDL_Color *out_hover,
                                           SDL_Color *out_active,
                                           SDL_Color *out_active_hover) {
    if (out_hover) {
        *out_hover = (SDL_Color){140, 150, 240, 255};
    }
    if (out_active) {
        *out_active = (SDL_Color){92, 142, 255, 255};
    }
    if (out_active_hover) {
        // Hovering the active editor should become lighter than active while staying blue.
        *out_active_hover = (SDL_Color){132, 178, 255, 255};
    }
}

bool ide_shared_font_resolve_role(CoreFontRoleId role,
                                  CoreFontTextSizeTier tier,
                                  char *out_path,
                                  size_t out_path_size,
                                  int *out_point_size) {
    const char *preset_name;
    CoreFontPreset preset = {0};
    CoreFontRoleSpec role_spec = {0};
    const char *selected_path = NULL;
    CoreResult r;

    if (!out_path || out_path_size == 0 || !out_point_size ||
        !is_shared_toggle_enabled("IDE_USE_SHARED_FONT")) {
        return false;
    }

    preset_name = getenv("IDE_FONT_PRESET");
    if (!preset_name || !preset_name[0]) {
        preset_name = "ide";
    }

    r = core_font_get_preset_by_name(preset_name, &preset);
    if (r.code != CORE_OK) {
        r = core_font_get_preset(CORE_FONT_PRESET_IDE, &preset);
        if (r.code != CORE_OK) {
            return false;
        }
    }

    r = core_font_resolve_role(&preset, role, &role_spec);
    if (r.code != CORE_OK) {
        return false;
    }

    r = core_font_choose_path(&role_spec, stat_path_exists, NULL, &selected_path);
    if (r.code != CORE_OK || !selected_path || !selected_path[0]) {
        return false;
    }

    r = core_font_point_size_for_tier(&role_spec, tier, out_point_size);
    if (r.code != CORE_OK) {
        return false;
    }

    strncpy(out_path, selected_path, out_path_size - 1);
    out_path[out_path_size - 1] = '\0';
    return true;
}
