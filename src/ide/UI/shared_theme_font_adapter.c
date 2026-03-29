#include "ide/UI/shared_theme_font_adapter.h"
#include "app/GlobalInfo/runtime_paths.h"

#include "core_theme.h"

#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static bool g_theme_runtime_initialized = false;
static CoreThemePresetId g_theme_runtime_preset = CORE_THEME_PRESET_IDE_GRAY;
static bool g_font_zoom_runtime_initialized = false;
static int g_font_zoom_step = 0;
static const CoreThemePresetId k_theme_cycle_order[] = {
    CORE_THEME_PRESET_DAW_DEFAULT,
    CORE_THEME_PRESET_MAP_FORGE_DEFAULT,
    CORE_THEME_PRESET_DARK_DEFAULT,
    CORE_THEME_PRESET_LIGHT_DEFAULT,
    CORE_THEME_PRESET_IDE_GRAY,
    CORE_THEME_PRESET_GREYSCALE
};
enum {
    IDE_FONT_ZOOM_STEP_MIN = -4,
    IDE_FONT_ZOOM_STEP_MAX = 5
};

static int clamp_int(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static void font_zoom_runtime_init_if_needed(void) {
    char* end = NULL;
    long parsed = 0;
    const char* env;
    if (g_font_zoom_runtime_initialized) {
        return;
    }
    env = getenv("IDE_FONT_ZOOM_STEP");
    if (env && env[0]) {
        parsed = strtol(env, &end, 10);
        if (end != env) {
            g_font_zoom_step = clamp_int((int)parsed, IDE_FONT_ZOOM_STEP_MIN, IDE_FONT_ZOOM_STEP_MAX);
        }
    }
    g_font_zoom_runtime_initialized = true;
}

static int font_zoom_step_percent(void) {
    int step = ide_shared_font_zoom_step();
    int pct = 100 + (step * 10);
    return clamp_int(pct, 60, 180);
}

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
    char resolved[PATH_MAX];
    char *resolved_out = (char*)user;
    if (!path || !path[0]) {
        return 0;
    }
    if (ide_runtime_probe_resource_path(path, resolved, sizeof(resolved))) {
        if (resolved_out) {
            strncpy(resolved_out, resolved, PATH_MAX - 1);
            resolved_out[PATH_MAX - 1] = '\0';
        }
        return 1;
    }
    if (stat(path, &st) == 0) {
        if (resolved_out) {
            strncpy(resolved_out, path, PATH_MAX - 1);
            resolved_out[PATH_MAX - 1] = '\0';
        }
        return 1;
    }
    return 0;
}

static void theme_runtime_init_if_needed(void) {
    const char *preset_name;
    CoreThemePresetId resolved_id;
    if (g_theme_runtime_initialized) {
        return;
    }
    preset_name = getenv("IDE_THEME_PRESET");
    if (preset_name && preset_name[0] &&
        core_theme_preset_id_from_name(preset_name, &resolved_id).code == CORE_OK) {
        g_theme_runtime_preset = resolved_id;
    } else {
        g_theme_runtime_preset = CORE_THEME_PRESET_IDE_GRAY;
    }
    g_theme_runtime_initialized = true;
}

static bool resolve_theme_preset(CoreThemePreset *out_preset) {
    CoreResult r;
    theme_runtime_init_if_needed();
    r = core_theme_get_preset(g_theme_runtime_preset, out_preset);
    if (r.code == CORE_OK) {
        return true;
    }
    r = core_theme_get_preset(CORE_THEME_PRESET_IDE_GRAY, out_preset);
    return r.code == CORE_OK;
}

bool ide_shared_theme_apply(UITheme *theme) {
    CoreThemePreset preset = {0};

    if (!theme || !is_shared_toggle_enabled("IDE_USE_SHARED_THEME")) {
        return false;
    }
    if (!resolve_theme_preset(&preset)) {
        return false;
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

bool ide_shared_theme_resolve_palette(IDEThemePalette *out_palette) {
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

    if (!out_palette) {
        return false;
    }

    ide_shared_theme_apply(&themed);

    out_palette->app_background = themed.bgEditor;
    out_palette->pane_header_fill = themed.bgMenuBar;
    out_palette->pane_body_fill = darken(themed.bgControlPanel, 14);
    out_palette->pane_border = themed.border;
    out_palette->button_fill = brighten(themed.bgMenuBar, 10);
    out_palette->button_fill_active = brighten(out_palette->button_fill, 18);
    out_palette->button_border = themed.border;
    out_palette->input_fill = brighten(themed.bgControlPanel, 12);
    out_palette->input_border = brighten(themed.bgControlPanel, 36);
    out_palette->input_focus_border = brighten(themed.border, 24);
    out_palette->modal_fill = themed.bgPopup;
    out_palette->modal_border = themed.border;
    out_palette->text_primary = themed.text;
    out_palette->text_muted = darken(themed.text, 55);
    out_palette->selection_fill = (SDL_Color){themed.border.r, themed.border.g, themed.border.b, 40};
    out_palette->accent_primary = themed.border;
    out_palette->accent_warning = (SDL_Color){232, 214, 162, 255};
    out_palette->accent_error = (SDL_Color){255, 96, 96, 255};
    return true;
}

bool ide_shared_theme_set_preset(const char *preset_name) {
    CoreThemePresetId id;
    if (!preset_name || !preset_name[0]) {
        return false;
    }
    if (core_theme_preset_id_from_name(preset_name, &id).code != CORE_OK) {
        return false;
    }
    g_theme_runtime_preset = id;
    g_theme_runtime_initialized = true;
    return true;
}

bool ide_shared_theme_current_preset(char *out_name, size_t out_name_size) {
    const char *name;
    if (!out_name || out_name_size == 0) {
        return false;
    }
    theme_runtime_init_if_needed();
    name = core_theme_preset_name(g_theme_runtime_preset);
    if (!name || !name[0]) {
        return false;
    }
    strncpy(out_name, name, out_name_size - 1);
    out_name[out_name_size - 1] = '\0';
    return true;
}

bool ide_shared_theme_cycle_next(void) {
    size_t i;
    size_t n = sizeof(k_theme_cycle_order) / sizeof(k_theme_cycle_order[0]);
    theme_runtime_init_if_needed();
    for (i = 0; i < n; ++i) {
        if (k_theme_cycle_order[i] == g_theme_runtime_preset) {
            g_theme_runtime_preset = k_theme_cycle_order[(i + 1u) % n];
            return true;
        }
    }
    g_theme_runtime_preset = k_theme_cycle_order[0];
    return true;
}

bool ide_shared_theme_cycle_prev(void) {
    size_t i;
    size_t n = sizeof(k_theme_cycle_order) / sizeof(k_theme_cycle_order[0]);
    theme_runtime_init_if_needed();
    for (i = 0; i < n; ++i) {
        if (k_theme_cycle_order[i] == g_theme_runtime_preset) {
            g_theme_runtime_preset = k_theme_cycle_order[(i + n - 1u) % n];
            return true;
        }
    }
    g_theme_runtime_preset = k_theme_cycle_order[0];
    return true;
}

SDL_Color ide_shared_theme_background_color(void) {
    IDEThemePalette palette = {0};
    if (ide_shared_theme_resolve_palette(&palette)) {
        return palette.app_background;
    }
    return (SDL_Color){30, 30, 30, 255};
}

void ide_shared_theme_button_colors(SDL_Color *out_fill,
                                    SDL_Color *out_fill_active,
                                    SDL_Color *out_border,
                                    SDL_Color *out_text) {
    IDEThemePalette palette = {0};
    if (ide_shared_theme_resolve_palette(&palette)) {
        if (out_fill) {
            *out_fill = palette.button_fill;
        }
        if (out_fill_active) {
            *out_fill_active = palette.button_fill_active;
        }
        if (out_border) {
            *out_border = palette.button_border;
        }
        if (out_text) {
            *out_text = palette.text_primary;
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

int ide_shared_font_zoom_step(void) {
    font_zoom_runtime_init_if_needed();
    return g_font_zoom_step;
}

bool ide_shared_font_set_zoom_step(int step) {
    int clamped = clamp_int(step, IDE_FONT_ZOOM_STEP_MIN, IDE_FONT_ZOOM_STEP_MAX);
    font_zoom_runtime_init_if_needed();
    if (clamped == g_font_zoom_step) {
        return false;
    }
    g_font_zoom_step = clamped;
    return true;
}

bool ide_shared_font_step_by(int delta) {
    return ide_shared_font_set_zoom_step(ide_shared_font_zoom_step() + delta);
}

bool ide_shared_font_reset_zoom_step(void) {
    return ide_shared_font_set_zoom_step(0);
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
    char selected_resolved[PATH_MAX] = {0};
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

    ide_runtime_paths_init(NULL);

    r = core_font_choose_path(&role_spec, stat_path_exists, selected_resolved, &selected_path);
    if (r.code != CORE_OK || !selected_path || !selected_path[0]) {
        return false;
    }

    r = core_font_point_size_for_tier(&role_spec, tier, out_point_size);
    if (r.code != CORE_OK) {
        return false;
    }
    {
        int percent = font_zoom_step_percent();
        int scaled = ((*out_point_size * percent) + 50) / 100;
        if (scaled < 6) scaled = 6;
        *out_point_size = scaled;
    }

    if (selected_resolved[0]) {
        strncpy(out_path, selected_resolved, out_path_size - 1);
    } else {
        strncpy(out_path, selected_path, out_path_size - 1);
    }
    out_path[out_path_size - 1] = '\0';
    return true;
}
