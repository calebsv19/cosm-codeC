#ifndef TERMINAL_GRID_INTERNAL_H
#define TERMINAL_GRID_INTERNAL_H

#include "terminal_grid.h"

#define ATTR_BOLD      (1 << 0)
#define ATTR_UNDERLINE (1 << 1)
#define ATTR_WIDE_CONTINUATION (1 << 7)

#define TERM_DEFAULT_FG 0xFFFFFFFFu
#define TERM_DEFAULT_BG 0x000000FFu

#define STATE_TEXT    0
#define STATE_ESC     1
#define STATE_CSI     2
#define STATE_OSC     3
#define STATE_OSC_ESC 4

void term_grid_sync_active_state(TermGrid* grid);
int term_grid_scroll_region_top(const TermGrid* grid);
int term_grid_scroll_region_bottom(const TermGrid* grid);
void term_grid_commit_active_state(TermGrid* grid);
void term_grid_fill_cells(TermCell* cells, int rows, int cols, uint32_t fg, uint32_t bg);
int term_grid_clamp_i(int v, int lo, int hi);
void term_grid_clear_scrollback(TermGrid* grid);
void term_grid_push_scrollback_row(TermGrid* grid, const TermCell* row_src);
void term_grid_clear_active_buffer(TermGrid* grid);

#endif
