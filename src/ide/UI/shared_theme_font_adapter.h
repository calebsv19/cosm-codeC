#ifndef IDE_SHARED_THEME_FONT_ADAPTER_H
#define IDE_SHARED_THEME_FONT_ADAPTER_H

#include "core_font.h"
#include "ide/Panes/PaneInfo/pane.h"

#include <stdbool.h>
#include <stddef.h>

bool ide_shared_theme_apply(UITheme *theme);
SDL_Color ide_shared_theme_background_color(void);
void ide_shared_theme_button_colors(SDL_Color *out_fill,
                                    SDL_Color *out_fill_active,
                                    SDL_Color *out_border,
                                    SDL_Color *out_text);
SDL_Color ide_shared_theme_pane_hover_border_color(void);
void ide_shared_theme_editor_border_colors(SDL_Color *out_hover,
                                           SDL_Color *out_active,
                                           SDL_Color *out_active_hover);
bool ide_shared_font_resolve_role(CoreFontRoleId role,
                                  CoreFontTextSizeTier tier,
                                  char *out_path,
                                  size_t out_path_size,
                                  int *out_point_size);

#endif
