#ifndef TERMINAL_GRID_SGR_HELPERS_H
#define TERMINAL_GRID_SGR_HELPERS_H

#include <stdint.h>

#include "terminal_grid.h"

uint32_t term_grid_pack_rgba(unsigned int r, unsigned int g, unsigned int b);
uint32_t term_grid_ansi16_color(unsigned int index);
uint32_t term_grid_ansi256_color(int index);
void term_grid_set_sgr_color(TermGrid* grid, int is_fg, uint32_t color);
void term_grid_reset_style(TermGrid* grid, uint32_t default_fg, uint32_t default_bg);
int term_grid_parse_int(const char* s, int len);

#endif // TERMINAL_GRID_SGR_HELPERS_H
