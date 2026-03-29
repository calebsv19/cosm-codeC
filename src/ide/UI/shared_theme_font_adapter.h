#ifndef IDE_SHARED_THEME_FONT_ADAPTER_H
#define IDE_SHARED_THEME_FONT_ADAPTER_H

#include "core_font.h"
#include "ide/Panes/PaneInfo/pane.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct IDEThemePalette {
    SDL_Color app_background;
    SDL_Color pane_header_fill;
    SDL_Color pane_body_fill;
    SDL_Color pane_border;
    SDL_Color button_fill;
    SDL_Color button_fill_active;
    SDL_Color button_border;
    SDL_Color input_fill;
    SDL_Color input_border;
    SDL_Color input_focus_border;
    SDL_Color modal_fill;
    SDL_Color modal_border;
    SDL_Color text_primary;
    SDL_Color text_muted;
    SDL_Color selection_fill;
    SDL_Color accent_primary;
    SDL_Color accent_warning;
    SDL_Color accent_error;
} IDEThemePalette;

bool ide_shared_theme_apply(UITheme *theme);
bool ide_shared_theme_resolve_palette(IDEThemePalette *out_palette);
SDL_Color ide_shared_theme_background_color(void);
bool ide_shared_theme_cycle_next(void);
bool ide_shared_theme_cycle_prev(void);
bool ide_shared_theme_set_preset(const char *preset_name);
bool ide_shared_theme_current_preset(char *out_name, size_t out_name_size);
void ide_shared_theme_button_colors(SDL_Color *out_fill,
                                    SDL_Color *out_fill_active,
                                    SDL_Color *out_border,
                                    SDL_Color *out_text);
SDL_Color ide_shared_theme_pane_hover_border_color(void);
void ide_shared_theme_editor_border_colors(SDL_Color *out_hover,
                                           SDL_Color *out_active,
                                           SDL_Color *out_active_hover);
int ide_shared_font_zoom_step(void);
bool ide_shared_font_set_zoom_step(int step);
bool ide_shared_font_step_by(int delta);
bool ide_shared_font_reset_zoom_step(void);
bool ide_shared_font_resolve_role(CoreFontRoleId role,
                                  CoreFontTextSizeTier tier,
                                  char *out_path,
                                  size_t out_path_size,
                                  int *out_point_size);

#endif
