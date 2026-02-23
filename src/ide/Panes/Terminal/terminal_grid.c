#include "terminal_grid.h"

#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define ATTR_BOLD      (1 << 0)
#define ATTR_UNDERLINE (1 << 1)

#define TERM_DEFAULT_FG 0xFFFFFFFFu
#define TERM_DEFAULT_BG 0x000000FFu

#define STATE_TEXT    0
#define STATE_ESC     1
#define STATE_CSI     2
#define STATE_OSC     3
#define STATE_OSC_ESC 4

static void term_grid_debug_log(const char* fmt, ...) {
    static int debug_enabled = -1;
    if (debug_enabled < 0) {
        const char* env = getenv("IDE_TERMINAL_DEBUG_PIPELINE");
        debug_enabled = (env && env[0] && strcmp(env, "0") != 0) ? 1 : 0;
    }
    if (!debug_enabled || !fmt) return;
    va_list args;
    va_start(args, fmt);
    printf("[TerminalGrid] ");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
}

static int* active_used_rows_ptr(TermGrid* grid) {
    if (!grid) return NULL;
    return grid->using_alternate ? &grid->alternate_used_rows : &grid->primary_used_rows;
}

static int* active_cursor_row_ptr(TermGrid* grid) {
    if (!grid) return NULL;
    return grid->using_alternate ? &grid->alternate_cursor_row : &grid->primary_cursor_row;
}

static int* active_cursor_col_ptr(TermGrid* grid) {
    if (!grid) return NULL;
    return grid->using_alternate ? &grid->alternate_cursor_col : &grid->primary_cursor_col;
}

static void sync_active_state(TermGrid* grid) {
    if (!grid) return;
    int* used = active_used_rows_ptr(grid);
    int* row = active_cursor_row_ptr(grid);
    int* col = active_cursor_col_ptr(grid);
    if (used) grid->used_rows = *used;
    if (row) grid->cursor_row = *row;
    if (col) grid->cursor_col = *col;
}

static void commit_active_state(TermGrid* grid) {
    if (!grid) return;
    int* used = active_used_rows_ptr(grid);
    int* row = active_cursor_row_ptr(grid);
    int* col = active_cursor_col_ptr(grid);
    if (used) *used = grid->used_rows;
    if (row) *row = grid->cursor_row;
    if (col) *col = grid->cursor_col;
}

static void fill_cells(TermCell* cells, int rows, int cols, uint32_t fg, uint32_t bg) {
    if (!cells || rows <= 0 || cols <= 0) return;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            TermCell* cell = &cells[r * cols + c];
            cell->ch = ' ';
            cell->fg = fg;
            cell->bg = bg;
            cell->attrs = 0;
        }
    }
}

static int clamp_i(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void resize_preserve_tail(const TermCell* oldCells, int oldRows, int oldCols,
                                 TermCell* newCells, int newRows, int newCols,
                                 int* io_used_rows, int* io_cursor_row, int* io_cursor_col) {
    if (!oldCells || !newCells || oldRows <= 0 || oldCols <= 0 || newRows <= 0 || newCols <= 0) return;
    if (!io_used_rows || !io_cursor_row || !io_cursor_col) return;

    int oldUsed = clamp_i(*io_used_rows, 1, oldRows);
    int copyRows = oldUsed < newRows ? oldUsed : newRows;
    int srcStart = oldUsed - copyRows;
    int dstStart = newRows - copyRows;
    int colsToCopy = oldCols < newCols ? oldCols : newCols;

    for (int r = 0; r < copyRows; ++r) {
        const TermCell* src = oldCells + (size_t)(srcStart + r) * (size_t)oldCols;
        TermCell* dst = newCells + (size_t)(dstStart + r) * (size_t)newCols;
        memcpy(dst, src, (size_t)colsToCopy * sizeof(TermCell));
    }

    int mappedRow = *io_cursor_row - srcStart + dstStart;
    *io_cursor_row = clamp_i(mappedRow, 0, newRows - 1);
    *io_cursor_col = clamp_i(*io_cursor_col, 0, newCols - 1);
    *io_used_rows = clamp_i(dstStart + copyRows, 1, newRows);
}

static void clear_scrollback(TermGrid* grid) {
    if (!grid) return;
    grid->scrollback_count = 0;
    grid->scrollback_head = 0;
    grid->scrollback_commit_count = 0;
    grid->scrollback_drop_count = 0;
    grid->alt_enter_count = 0;
    grid->alt_exit_count = 0;
    grid->alt_ignored_count = 0;
}

static void push_scrollback_row(TermGrid* grid, const TermCell* row_src) {
    if (!grid || !row_src || grid->scrollback_cap_rows <= 0 || grid->cols <= 0) return;
    if (!grid->scrollback_cells) return;
    size_t rowBytes = (size_t)grid->cols * sizeof(TermCell);

    if (grid->scrollback_count < grid->scrollback_cap_rows) {
        int idx = (grid->scrollback_head + grid->scrollback_count) % grid->scrollback_cap_rows;
        memcpy(grid->scrollback_cells + (size_t)idx * (size_t)grid->cols, row_src, rowBytes);
        grid->scrollback_count++;
        grid->scrollback_commit_count++;
        return;
    }

    memcpy(grid->scrollback_cells + (size_t)grid->scrollback_head * (size_t)grid->cols, row_src, rowBytes);
    grid->scrollback_head = (grid->scrollback_head + 1) % grid->scrollback_cap_rows;
    grid->scrollback_commit_count++;
    grid->scrollback_drop_count++;
}

static void clear_active_buffer(TermGrid* grid) {
    if (!grid || !grid->cells || grid->rows <= 0 || grid->cols <= 0) return;
    fill_cells(grid->cells, grid->rows, grid->cols, grid->cur_fg, grid->cur_bg);
    grid->cursor_row = 0;
    grid->cursor_col = 0;
    grid->used_rows = 1;
    commit_active_state(grid);
}

static void grid_alloc(TermGrid* grid, int rows, int cols) {
    if (!grid || rows <= 0 || cols <= 0) return;
    grid->rows = rows;
    grid->cols = cols;
    grid->primary_cells = (TermCell*)malloc((size_t)rows * (size_t)cols * sizeof(TermCell));
    grid->alternate_cells = NULL;
    grid->using_alternate = 0;
    grid->alternate_screen_enabled = 1;
    grid->cells = grid->primary_cells;
    grid->scrollback_cells = NULL;
    grid->scrollback_cap_rows = 0;
    grid->scrollback_count = 0;
    grid->scrollback_head = 0;
    grid->scrollback_commit_count = 0;
    grid->scrollback_drop_count = 0;
    term_grid_clear(grid);
}

static void clamp_cursor(TermGrid* grid);
static void term_grid_restore_cursor(TermGrid* grid);

static inline uint32_t pack_rgba(unsigned int r, unsigned int g, unsigned int b) {
    return ((r & 0xFFu) << 24) | ((g & 0xFFu) << 16) | ((b & 0xFFu) << 8) | 0xFFu;
}

static uint32_t ansi16_color(unsigned int index) {
    static const uint32_t base[16] = {
        0x000000FFu, // black
        0xAA0000FFu, // red
        0x00AA00FFu, // green
        0xAA5500FFu, // yellow
        0x0000AAFFu, // blue
        0xAA00AAFFu, // magenta
        0x00AAAAFFu, // cyan
        0xAAAAAAAAu, // white/gray
        0x555555FFu, // bright black
        0xFF5555FFu, // bright red
        0x55FF55FFu, // bright green
        0xFFFF55FFu, // bright yellow
        0x5555FFFFu, // bright blue
        0xFF55FFFFu, // bright magenta
        0x55FFFFFFu, // bright cyan
        0xFFFFFFFFu, // bright white
    };
    return base[index & 0x0Fu];
}

static uint32_t ansi256_color(int index) {
    if (index < 0) index = 0;
    if (index > 255) index = 255;

    if (index < 16) {
        return ansi16_color((unsigned int)index);
    }

    if (index <= 231) {
        int n = index - 16;
        int r = n / 36;
        int g = (n / 6) % 6;
        int b = n % 6;

        unsigned int rr = (r == 0) ? 0u : (unsigned int)(55 + r * 40);
        unsigned int gg = (g == 0) ? 0u : (unsigned int)(55 + g * 40);
        unsigned int bb = (b == 0) ? 0u : (unsigned int)(55 + b * 40);
        return pack_rgba(rr, gg, bb);
    }

    unsigned int v = (unsigned int)(8 + (index - 232) * 10);
    return pack_rgba(v, v, v);
}

static void set_sgr_color(TermGrid* grid, int is_fg, uint32_t color) {
    if (is_fg) {
        grid->cur_fg = color;
    } else {
        grid->cur_bg = color;
    }
}

static void reset_style(TermGrid* grid) {
    grid->cur_fg = TERM_DEFAULT_FG;
    grid->cur_bg = TERM_DEFAULT_BG;
    grid->cur_attrs = 0;
}

static int parse_int(const char* s, int len) {
    int v = 0;
    for (int i = 0; i < len; ++i) {
        if (!isdigit((unsigned char)s[i])) return v;
        v = v * 10 + (s[i] - '0');
    }
    return v;
}

void term_grid_init(TermGrid* grid, int rows, int cols) {
    if (!grid) return;
    memset(grid, 0, sizeof(*grid));
    grid->cur_fg = TERM_DEFAULT_FG;
    grid->cur_bg = TERM_DEFAULT_BG;
    grid->parser_state = STATE_TEXT;
    grid->viewport_rows = rows;
    grid->viewport_cols = cols;
    grid_alloc(grid, rows, cols);
}

void term_grid_free(TermGrid* grid) {
    if (!grid) return;
    free(grid->primary_cells);
    free(grid->alternate_cells);
    free(grid->scrollback_cells);
    grid->cells = NULL;
    grid->primary_cells = NULL;
    grid->alternate_cells = NULL;
    grid->scrollback_cells = NULL;
    grid->rows = grid->cols = 0;
}

void term_grid_clear(TermGrid* grid) {
    if (!grid || !grid->cells) return;
    fill_cells(grid->cells, grid->rows, grid->cols, grid->cur_fg, grid->cur_bg);
    grid->cursor_row = 0;
    grid->cursor_col = 0;
    grid->used_rows = 1;
    clear_scrollback(grid);
    commit_active_state(grid);
}

void term_grid_clear_line(TermGrid* grid, int row) {
    if (!grid || !grid->cells) return;
    if (row < 0 || row >= grid->rows) return;
    for (int c = 0; c < grid->cols; ++c) {
        TermCell* cell = term_grid_cell(grid, row, c);
        cell->ch = ' ';
        cell->fg = grid->cur_fg;
        cell->bg = grid->cur_bg;
        cell->attrs = 0;
    }
}

void term_grid_resize(TermGrid* grid, int rows, int cols) {
    if (!grid || rows <= 0 || cols <= 0) return;
    if (!grid->cells) {
        term_grid_init(grid, rows, cols);
        return;
    }
    int oldRows = grid->rows;
    int oldCols = grid->cols;
    TermCell* oldPrimary = grid->primary_cells;
    TermCell* oldAlternate = grid->alternate_cells;
    TermCell* newPrimary = (TermCell*)malloc((size_t)rows * (size_t)cols * sizeof(TermCell));
    if (!newPrimary) return;
    fill_cells(newPrimary, rows, cols, grid->cur_fg, grid->cur_bg);

    TermCell* newAlternate = NULL;
    if (oldAlternate) {
        newAlternate = (TermCell*)malloc((size_t)rows * (size_t)cols * sizeof(TermCell));
        if (!newAlternate) {
            free(newPrimary);
            return;
        }
        fill_cells(newAlternate, rows, cols, grid->cur_fg, grid->cur_bg);
    }

    int primaryUsed = grid->primary_used_rows;
    int primaryRow = grid->primary_cursor_row;
    int primaryCol = grid->primary_cursor_col;
    resize_preserve_tail(oldPrimary, oldRows, oldCols, newPrimary, rows, cols,
                         &primaryUsed, &primaryRow, &primaryCol);

    int alternateUsed = grid->alternate_used_rows;
    int alternateRow = grid->alternate_cursor_row;
    int alternateCol = grid->alternate_cursor_col;
    if (oldAlternate && newAlternate) {
        resize_preserve_tail(oldAlternate, oldRows, oldCols, newAlternate, rows, cols,
                             &alternateUsed, &alternateRow, &alternateCol);
    }

    free(oldPrimary);
    free(oldAlternate);
    grid->primary_cells = newPrimary;
    grid->alternate_cells = newAlternate;
    grid->primary_used_rows = primaryUsed;
    grid->primary_cursor_row = primaryRow;
    grid->primary_cursor_col = primaryCol;
    if (newAlternate) {
        grid->alternate_used_rows = alternateUsed;
        grid->alternate_cursor_row = alternateRow;
        grid->alternate_cursor_col = alternateCol;
    } else {
        grid->alternate_used_rows = 1;
        grid->alternate_cursor_row = 0;
        grid->alternate_cursor_col = 0;
    }
    grid->rows = rows;
    grid->cols = cols;
    if (grid->viewport_rows < 1 || grid->viewport_rows > rows) grid->viewport_rows = rows;
    if (grid->viewport_cols < 1 || grid->viewport_cols > cols) grid->viewport_cols = cols;
    grid->cells = grid->using_alternate ? grid->alternate_cells : grid->primary_cells;
    if (!grid->cells) grid->cells = grid->primary_cells;
    if (grid->scrollback_cells && oldCols != cols && grid->scrollback_cap_rows > 0) {
        TermCell* oldRing = grid->scrollback_cells;
        TermCell* newRing = (TermCell*)malloc((size_t)grid->scrollback_cap_rows * (size_t)cols * sizeof(TermCell));
        if (newRing) {
            fill_cells(newRing, grid->scrollback_cap_rows, cols, TERM_DEFAULT_FG, TERM_DEFAULT_BG);
            int rowsToCopy = grid->scrollback_count;
            int colsToCopy = oldCols < cols ? oldCols : cols;
            for (int i = 0; i < rowsToCopy; ++i) {
                int oldIdx = (grid->scrollback_head + i) % grid->scrollback_cap_rows;
                const TermCell* src = oldRing + (size_t)oldIdx * (size_t)oldCols;
                TermCell* dst = newRing + (size_t)i * (size_t)cols;
                memcpy(dst, src, (size_t)colsToCopy * sizeof(TermCell));
            }
            free(oldRing);
            grid->scrollback_cells = newRing;
            grid->scrollback_head = 0;
        }
    }

    sync_active_state(grid);
    clamp_cursor(grid);
    if (grid->used_rows < 1) grid->used_rows = 1;
    if (grid->used_rows > grid->rows) grid->used_rows = grid->rows;
    if (grid->cursor_row + 1 > grid->used_rows) grid->used_rows = grid->cursor_row + 1;
    commit_active_state(grid);
}

void term_grid_set_viewport_size(TermGrid* grid, int rows, int cols) {
    if (!grid) return;
    if (rows > 0) {
        grid->viewport_rows = rows;
        if (grid->viewport_rows > grid->rows) grid->viewport_rows = grid->rows;
    }
    if (cols > 0) {
        grid->viewport_cols = cols;
        if (grid->viewport_cols > grid->cols) grid->viewport_cols = grid->cols;
    }
    if (grid->viewport_rows < 1) grid->viewport_rows = 1;
    if (grid->viewport_cols < 1) grid->viewport_cols = 1;
}

void term_grid_set_alternate_screen_enabled(TermGrid* grid, int enabled) {
    if (!grid) return;
    grid->alternate_screen_enabled = enabled ? 1 : 0;
    if (!grid->alternate_screen_enabled && grid->using_alternate) {
        term_grid_debug_log("alt force-exit due to disable");
        commit_active_state(grid);
        grid->using_alternate = 0;
        grid->cells = grid->primary_cells;
        sync_active_state(grid);
        term_grid_restore_cursor(grid);
        clamp_cursor(grid);
        grid->alt_exit_count++;
    }
}

void term_grid_set_scrollback_cap(TermGrid* grid, int cap_rows) {
    if (!grid) return;
    if (cap_rows < 0) cap_rows = 0;
    if (cap_rows == grid->scrollback_cap_rows) return;

    if (cap_rows == 0 || grid->cols <= 0) {
        free(grid->scrollback_cells);
        grid->scrollback_cells = NULL;
        grid->scrollback_cap_rows = 0;
        grid->scrollback_count = 0;
        grid->scrollback_head = 0;
        grid->scrollback_commit_count = 0;
        grid->scrollback_drop_count = 0;
        return;
    }

    TermCell* newCells = (TermCell*)malloc((size_t)cap_rows * (size_t)grid->cols * sizeof(TermCell));
    if (!newCells) return;

    int copyCount = grid->scrollback_count;
    if (copyCount > cap_rows) copyCount = cap_rows;
    int start = grid->scrollback_count - copyCount;
    if (start < 0) start = 0;

    for (int i = 0; i < copyCount; ++i) {
        int oldIdx = (grid->scrollback_head + start + i) % grid->scrollback_cap_rows;
        const TermCell* src = grid->scrollback_cells + (size_t)oldIdx * (size_t)grid->cols;
        TermCell* dst = newCells + (size_t)i * (size_t)grid->cols;
        memcpy(dst, src, (size_t)grid->cols * sizeof(TermCell));
    }

    free(grid->scrollback_cells);
    grid->scrollback_cells = newCells;
    grid->scrollback_cap_rows = cap_rows;
    grid->scrollback_count = copyCount;
    grid->scrollback_head = 0;
}

int term_grid_scrollback_count(const TermGrid* grid) {
    if (!grid) return 0;
    return grid->scrollback_count;
}

const TermCell* term_grid_scrollback_row(const TermGrid* grid, int index) {
    if (!grid || !grid->scrollback_cells || grid->scrollback_cap_rows <= 0) return NULL;
    if (index < 0 || index >= grid->scrollback_count) return NULL;
    int ringIndex = (grid->scrollback_head + index) % grid->scrollback_cap_rows;
    return grid->scrollback_cells + (size_t)ringIndex * (size_t)grid->cols;
}

unsigned long long term_grid_scrollback_commit_count(const TermGrid* grid) {
    if (!grid) return 0ull;
    return grid->scrollback_commit_count;
}

unsigned long long term_grid_scrollback_drop_count(const TermGrid* grid) {
    if (!grid) return 0ull;
    return grid->scrollback_drop_count;
}

unsigned long long term_grid_alt_enter_count(const TermGrid* grid) {
    if (!grid) return 0ull;
    return grid->alt_enter_count;
}

unsigned long long term_grid_alt_exit_count(const TermGrid* grid) {
    if (!grid) return 0ull;
    return grid->alt_exit_count;
}

unsigned long long term_grid_alt_ignored_count(const TermGrid* grid) {
    if (!grid) return 0ull;
    return grid->alt_ignored_count;
}

static void clamp_cursor(TermGrid* grid) {
    if (!grid) return;
    if (grid->cursor_row < 0) grid->cursor_row = 0;
    if (grid->cursor_row >= grid->rows) grid->cursor_row = grid->rows - 1;
    if (grid->cursor_col < 0) grid->cursor_col = 0;
    if (grid->cursor_col >= grid->cols) grid->cursor_col = grid->cols - 1;
    commit_active_state(grid);
}

static void scroll_up(TermGrid* grid) {
    if (!grid || !grid->cells || grid->rows <= 0) return;
    if (!grid->using_alternate && grid->cols > 0) {
        push_scrollback_row(grid, grid->cells);
    }

    memmove(grid->cells,
            grid->cells + grid->cols,
            (size_t)(grid->rows - 1) * (size_t)grid->cols * sizeof(TermCell));

    for (int c = 0; c < grid->cols; ++c) {
        TermCell* cell = term_grid_cell(grid, grid->rows - 1, c);
        cell->ch = ' ';
        cell->fg = grid->cur_fg;
        cell->bg = grid->cur_bg;
        cell->attrs = 0;
    }

    if (grid->cursor_row > 0) grid->cursor_row--;
}

static void move_cursor_newline(TermGrid* grid) {
    grid->cursor_col = 0;
    grid->cursor_row++;
    if (grid->cursor_row + 1 > grid->used_rows) {
        grid->used_rows = grid->cursor_row + 1;
    }
    if (grid->cursor_row >= grid->rows) {
        scroll_up(grid);
        grid->cursor_row = grid->rows - 1;
        grid->used_rows = grid->rows;
    }
}

static void write_codepoint(TermGrid* grid, uint32_t cp) {
    if (!grid || !grid->cells) return;

    if (cp == '\t') {
        int next_tab = ((grid->cursor_col / 8) + 1) * 8;
        if (next_tab <= grid->cursor_col) next_tab = grid->cursor_col + 1;
        while (grid->cursor_col < next_tab) {
            write_codepoint(grid, ' ');
        }
        return;
    }

    if (cp < 0x20u || cp == 0x7Fu) {
        return;
    }

    TermCell* cell = term_grid_cell(grid, grid->cursor_row, grid->cursor_col);
    if (cell) {
        cell->ch = cp;
        cell->fg = grid->cur_fg;
        cell->bg = grid->cur_bg;
        cell->attrs = grid->cur_attrs;
    }
    if (grid->cursor_row + 1 > grid->used_rows) {
        grid->used_rows = grid->cursor_row + 1;
    }

    grid->cursor_col++;
    if (grid->cursor_col >= grid->cols) {
        move_cursor_newline(grid);
    }
}

static int utf8_feed_byte(TermGrid* grid, unsigned char byte, uint32_t* out_cp) {
    if (!grid || !out_cp) return 0;

    if (grid->utf8_expected == 0) {
        if (byte < 0x80u) {
            *out_cp = (uint32_t)byte;
            return 1;
        }
        if (byte >= 0xC2u && byte <= 0xDFu) {
            grid->utf8_codepoint = (uint32_t)(byte & 0x1Fu);
            grid->utf8_expected = 1;
            grid->utf8_seen = 0;
            return 0;
        }
        if (byte >= 0xE0u && byte <= 0xEFu) {
            grid->utf8_codepoint = (uint32_t)(byte & 0x0Fu);
            grid->utf8_expected = 2;
            grid->utf8_seen = 0;
            return 0;
        }
        if (byte >= 0xF0u && byte <= 0xF4u) {
            grid->utf8_codepoint = (uint32_t)(byte & 0x07u);
            grid->utf8_expected = 3;
            grid->utf8_seen = 0;
            return 0;
        }
        *out_cp = 0xFFFDu;
        return 1;
    }

    if ((byte & 0xC0u) != 0x80u) {
        // Invalid continuation byte: emit replacement for the broken sequence,
        // reset, and let caller process this byte again as a fresh starter.
        grid->utf8_expected = 0;
        grid->utf8_seen = 0;
        grid->utf8_codepoint = 0;
        *out_cp = 0xFFFDu;
        return 1;
    }

    grid->utf8_codepoint = (grid->utf8_codepoint << 6) | (uint32_t)(byte & 0x3Fu);
    grid->utf8_seen++;

    if (grid->utf8_seen < grid->utf8_expected) {
        return 0;
    }

    uint32_t cp = grid->utf8_codepoint;
    uint8_t expected = grid->utf8_expected;
    grid->utf8_expected = 0;
    grid->utf8_seen = 0;
    grid->utf8_codepoint = 0;

    if ((expected == 1 && cp < 0x80u) ||
        (expected == 2 && cp < 0x800u) ||
        (expected == 3 && cp < 0x10000u) ||
        (cp >= 0xD800u && cp <= 0xDFFFu) ||
        (cp > 0x10FFFFu)) {
        *out_cp = 0xFFFDu;
        return 1;
    }

    *out_cp = cp;
    return 1;
}

static void apply_sgr(TermGrid* grid, const int* params, int count) {
    if (!grid || count == 0) {
        reset_style(grid);
        return;
    }

    for (int i = 0; i < count; ++i) {
        int p = params[i];

        if (p == 0) {
            reset_style(grid);
        } else if (p == 1) {
            grid->cur_attrs |= ATTR_BOLD;
        } else if (p == 4) {
            grid->cur_attrs |= ATTR_UNDERLINE;
        } else if (p == 22) {
            grid->cur_attrs &= (uint8_t)~ATTR_BOLD;
        } else if (p == 24) {
            grid->cur_attrs &= (uint8_t)~ATTR_UNDERLINE;
        } else if (p == 39) {
            grid->cur_fg = TERM_DEFAULT_FG;
        } else if (p == 49) {
            grid->cur_bg = TERM_DEFAULT_BG;
        } else if (p >= 30 && p <= 37) {
            set_sgr_color(grid, 1, ansi16_color((unsigned int)(p - 30)));
        } else if (p >= 40 && p <= 47) {
            set_sgr_color(grid, 0, ansi16_color((unsigned int)(p - 40)));
        } else if (p >= 90 && p <= 97) {
            set_sgr_color(grid, 1, ansi16_color((unsigned int)(8 + (p - 90))));
        } else if (p >= 100 && p <= 107) {
            set_sgr_color(grid, 0, ansi16_color((unsigned int)(8 + (p - 100))));
        } else if (p == 38 || p == 48) {
            int is_fg = (p == 38);
            if (i + 1 >= count) continue;

            int mode = params[++i];
            if (mode == 5) {
                if (i + 1 >= count) continue;
                int idx = params[++i];
                set_sgr_color(grid, is_fg, ansi256_color(idx));
            } else if (mode == 2) {
                if (i + 3 >= count) continue;
                int r = params[++i];
                int g = params[++i];
                int b = params[++i];
                if (r < 0) r = 0; else if (r > 255) r = 255;
                if (g < 0) g = 0; else if (g > 255) g = 255;
                if (b < 0) b = 0; else if (b > 255) b = 255;
                set_sgr_color(grid, is_fg, pack_rgba((unsigned int)r, (unsigned int)g, (unsigned int)b));
            }
        }
    }
}

static void term_grid_save_cursor(TermGrid* grid) {
    if (!grid) return;
    grid->saved_cursor_row = grid->cursor_row;
    grid->saved_cursor_col = grid->cursor_col;
    grid->has_saved_cursor = 1;
    term_grid_debug_log("cursor save r=%d c=%d mode=%s",
                        grid->saved_cursor_row,
                        grid->saved_cursor_col,
                        grid->using_alternate ? "alt" : "primary");
}

static void term_grid_restore_cursor(TermGrid* grid) {
    if (!grid || !grid->has_saved_cursor) return;
    grid->cursor_row = grid->saved_cursor_row;
    grid->cursor_col = grid->saved_cursor_col;
    clamp_cursor(grid);
    term_grid_debug_log("cursor restore r=%d c=%d mode=%s",
                        grid->cursor_row,
                        grid->cursor_col,
                        grid->using_alternate ? "alt" : "primary");
}

static void term_grid_set_alternate(TermGrid* grid, int enable, int clearOnEnter,
                                    int saveCursor, int restoreCursor) {
    if (!grid || !grid->primary_cells) return;
    if (saveCursor) {
        term_grid_save_cursor(grid);
    }
    if (enable) {
        if (grid->using_alternate) {
            term_grid_debug_log("alt re-enter clear=%d", clearOnEnter ? 1 : 0);
        } else {
            term_grid_debug_log("alt enter clear=%d", clearOnEnter ? 1 : 0);
            grid->alt_enter_count++;
        }
        commit_active_state(grid);
        if (!grid->alternate_cells) {
            grid->alternate_cells =
                (TermCell*)malloc((size_t)grid->rows * (size_t)grid->cols * sizeof(TermCell));
            if (!grid->alternate_cells) return;
            fill_cells(grid->alternate_cells, grid->rows, grid->cols, grid->cur_fg, grid->cur_bg);
            grid->alternate_cursor_row = 0;
            grid->alternate_cursor_col = 0;
            grid->alternate_used_rows = 1;
        } else if (clearOnEnter) {
            fill_cells(grid->alternate_cells, grid->rows, grid->cols, grid->cur_fg, grid->cur_bg);
            grid->alternate_cursor_row = 0;
            grid->alternate_cursor_col = 0;
            grid->alternate_used_rows = 1;
        }
        grid->using_alternate = 1;
        grid->cells = grid->alternate_cells;
        sync_active_state(grid);
        clamp_cursor(grid);
        return;
    }

    commit_active_state(grid);
    grid->using_alternate = 0;
    grid->cells = grid->primary_cells;
    sync_active_state(grid);
    if (restoreCursor) {
        term_grid_restore_cursor(grid);
    }
    clamp_cursor(grid);
    grid->alt_exit_count++;
    term_grid_debug_log("alt exit restore=%d", restoreCursor ? 1 : 0);
}

static void handle_private_mode(TermGrid* grid, const int* values, int count, int setMode) {
    if (!grid || !values || count <= 0) return;
    for (int i = 0; i < count; ++i) {
        int mode = values[i];
        switch (mode) {
            case 1047:
                if (!grid->alternate_screen_enabled) {
                    grid->alt_ignored_count++;
                    term_grid_debug_log("ignore private mode ?1047%c (alt disabled)", setMode ? 'h' : 'l');
                    break;
                }
                term_grid_set_alternate(grid, setMode, setMode, 0, 0);
                break;
            case 1048:
                if (setMode) term_grid_save_cursor(grid);
                else term_grid_restore_cursor(grid);
                break;
            case 1049:
                if (!grid->alternate_screen_enabled) {
                    grid->alt_ignored_count++;
                    term_grid_debug_log("ignore private mode ?1049%c (alt disabled)", setMode ? 'h' : 'l');
                    break;
                }
                term_grid_set_alternate(grid, setMode, setMode, 1, !setMode);
                break;
            default:
                break;
        }
    }
}

static void handle_csi(TermGrid* grid, const char* params, int paramLen, char command) {
    int privateMode = 0;
    const char* p = params;
    int plen = paramLen;
    if (plen > 0 && p[0] == '?') {
        privateMode = 1;
        p++;
        plen--;
    }

    int values[32] = {0};
    int count = 0;

    int start = 0;
    for (int i = 0; i <= plen; ++i) {
        if (i == plen || p[i] == ';') {
            if (count < (int)(sizeof(values) / sizeof(values[0]))) {
                values[count++] = parse_int(p + start, i - start);
            }
            start = i + 1;
        }
    }
    if (count == 0) {
        values[count++] = 0;
    }

    if (privateMode && (command == 'h' || command == 'l')) {
        handle_private_mode(grid, values, count, command == 'h');
        return;
    }

    int n = values[0] ? values[0] : 1;
    switch (command) {
        case 'A': // CUU
            grid->cursor_row -= n;
            clamp_cursor(grid);
            break;
        case 'B': // CUD
            grid->cursor_row += n;
            if (grid->cursor_row >= grid->rows) grid->cursor_row = grid->rows - 1;
            break;
        case 'C': // CUF
            grid->cursor_col += n;
            if (grid->cursor_col >= grid->cols) grid->cursor_col = grid->cols - 1;
            break;
        case 'D': // CUB
            grid->cursor_col -= n;
            if (grid->cursor_col < 0) grid->cursor_col = 0;
            break;
        case 'E': // CNL
            grid->cursor_row += n;
            if (grid->cursor_row >= grid->rows) grid->cursor_row = grid->rows - 1;
            grid->cursor_col = 0;
            break;
        case 'F': // CPL
            grid->cursor_row -= n;
            if (grid->cursor_row < 0) grid->cursor_row = 0;
            grid->cursor_col = 0;
            break;
        case 'G': { // CHA
            int col = (values[0] > 0) ? values[0] - 1 : 0;
            if (col < 0) col = 0;
            if (col >= grid->cols) col = grid->cols - 1;
            grid->cursor_col = col;
            break;
        }
        case 'H': // CUP
        case 'f': { // HVP
            int row = (count >= 1 && values[0] > 0) ? values[0] - 1 : 0;
            int col = (count >= 2 && values[1] > 0) ? values[1] - 1 : 0;
            grid->cursor_row = row;
            grid->cursor_col = col;
            clamp_cursor(grid);
            break;
        }
        case 'd': { // VPA
            int row = (values[0] > 0) ? values[0] - 1 : 0;
            if (row < 0) row = 0;
            if (row >= grid->rows) row = grid->rows - 1;
            grid->cursor_row = row;
            break;
        }
        case 'J': { // ED
            int mode = values[0];
            if (mode == 2) {
                if (grid->using_alternate) {
                    // Clear only the active alternate buffer; never touch primary scrollback.
                    clear_active_buffer(grid);
                } else {
                    int viewRows = grid->viewport_rows;
                    if (viewRows < 1 || viewRows > grid->rows) viewRows = grid->rows;
                    int startRow = grid->cursor_row - (viewRows - 1);
                    if (startRow < 0) startRow = 0;
                    int endRow = startRow + viewRows - 1;
                    if (endRow >= grid->rows) endRow = grid->rows - 1;
                    for (int r = startRow; r <= endRow; ++r) {
                        term_grid_clear_line(grid, r);
                    }
                }
            } else if (mode == 0) {
                for (int c = grid->cursor_col; c < grid->cols; ++c) {
                    TermCell* cell = term_grid_cell(grid, grid->cursor_row, c);
                    if (cell) {
                        cell->ch = ' ';
                        cell->fg = grid->cur_fg;
                        cell->bg = grid->cur_bg;
                        cell->attrs = 0;
                    }
                }
                for (int r = grid->cursor_row + 1; r < grid->rows; ++r) {
                    term_grid_clear_line(grid, r);
                }
            } else if (mode == 1) {
                for (int c = 0; c <= grid->cursor_col && c < grid->cols; ++c) {
                    TermCell* cell = term_grid_cell(grid, grid->cursor_row, c);
                    if (cell) {
                        cell->ch = ' ';
                        cell->fg = grid->cur_fg;
                        cell->bg = grid->cur_bg;
                        cell->attrs = 0;
                    }
                }
                for (int r = 0; r < grid->cursor_row; ++r) {
                    term_grid_clear_line(grid, r);
                }
            }
            break;
        }
        case 'K': { // EL
            int mode = values[0];
            if (mode == 0 || mode == 1) {
                int startCol = (mode == 0) ? grid->cursor_col : 0;
                int endCol = (mode == 0) ? grid->cols - 1 : grid->cursor_col;
                if (startCol < 0) startCol = 0;
                if (endCol >= grid->cols) endCol = grid->cols - 1;
                for (int c = startCol; c <= endCol; ++c) {
                    TermCell* cell = term_grid_cell(grid, grid->cursor_row, c);
                    if (cell) {
                        cell->ch = ' ';
                        cell->fg = grid->cur_fg;
                        cell->bg = grid->cur_bg;
                        cell->attrs = 0;
                    }
                }
            } else if (mode == 2) {
                term_grid_clear_line(grid, grid->cursor_row);
            }
            break;
        }
        case 'm': // SGR
            apply_sgr(grid, values, count);
            break;
        default:
            break;
    }
    if (grid->cursor_row + 1 > grid->used_rows) {
        grid->used_rows = grid->cursor_row + 1;
        if (grid->used_rows > grid->rows) grid->used_rows = grid->rows;
    }
}

void term_emulator_feed(TermGrid* grid, const char* data, size_t len) {
    if (!grid || !grid->cells || !data || len == 0) return;

    for (size_t i = 0; i < len; ++i) {
        unsigned char ch = (unsigned char)data[i];

        switch (grid->parser_state) {
            case STATE_TEXT: {
                if (ch == 0x1Bu) {
                    grid->parser_state = STATE_ESC;
                    break;
                }
                if (ch == '\n') {
                    move_cursor_newline(grid);
                    break;
                }
                if (ch == '\r') {
                    // Always honor carriage-return semantics: rewrite current row from column 0.
                    // This keeps spinner/progress updates and shell redraws from creating fake rows.
                    grid->cursor_col = 0;
                    break;
                }
                if (ch == '\b' || ch == 0x7Fu) {
                    if (grid->cursor_col > 0) grid->cursor_col--;
                    break;
                }

                uint32_t cp = 0;
                int produced = utf8_feed_byte(grid, ch, &cp);
                if (produced) {
                    write_codepoint(grid, cp);
                    // If the previous sequence was invalid, retry this byte as a starter.
                    if (cp == 0xFFFDu && ch >= 0x80u && grid->utf8_expected == 0) {
                        uint32_t retry = 0;
                        if (utf8_feed_byte(grid, ch, &retry)) {
                            if (retry != 0xFFFDu || ch < 0x80u) {
                                write_codepoint(grid, retry);
                            }
                        }
                    }
                }
                break;
            }

            case STATE_ESC:
                if (ch == '[') {
                    grid->parser_state = STATE_CSI;
                    grid->csi_len = 0;
                } else if (ch == ']') {
                    grid->parser_state = STATE_OSC;
                } else if (ch == '7') {
                    term_grid_save_cursor(grid);
                    grid->parser_state = STATE_TEXT;
                } else if (ch == '8') {
                    term_grid_restore_cursor(grid);
                    grid->parser_state = STATE_TEXT;
                } else {
                    grid->parser_state = STATE_TEXT;
                }
                break;

            case STATE_CSI:
                if (ch >= 0x40u && ch <= 0x7Eu) {
                    handle_csi(grid, grid->csi_buf, grid->csi_len, (char)ch);
                    grid->csi_len = 0;
                    grid->parser_state = STATE_TEXT;
                } else if (grid->csi_len < (int)sizeof(grid->csi_buf) - 1) {
                    grid->csi_buf[grid->csi_len++] = (char)ch;
                }
                break;

            case STATE_OSC:
                if (ch == 0x07u) {
                    grid->parser_state = STATE_TEXT;
                } else if (ch == 0x1Bu) {
                    grid->parser_state = STATE_OSC_ESC;
                }
                break;

            case STATE_OSC_ESC:
                if (ch == '\\' || ch == 0x07u) {
                    grid->parser_state = STATE_TEXT;
                } else if (ch == 0x1Bu) {
                    grid->parser_state = STATE_OSC_ESC;
                } else {
                    grid->parser_state = STATE_OSC;
                }
                break;

            default:
                grid->parser_state = STATE_TEXT;
                break;
        }
    }
}

void term_grid_debug_render(TermGrid* grid, int x, int y, int lineHeight, int maxHeight,
                            void (*drawFn)(int, int, const char*)) {
    if (!grid || !grid->cells || !drawFn) return;
    int rowsVisible = (lineHeight > 0) ? maxHeight / lineHeight : grid->rows;
    if (rowsVisible > grid->rows) rowsVisible = grid->rows;
    int endRow = grid->rows;
    int startRow = (endRow - rowsVisible >= 0) ? endRow - rowsVisible : 0;
    for (int r = startRow; r < endRow; ++r) {
        char lineBuf[512];
        int w = 0;
        for (int c = 0; c < grid->cols && w < (int)sizeof(lineBuf) - 1; ++c) {
            TermCell* cell = term_grid_cell(grid, r, c);
            uint32_t ch = cell ? (cell->ch ? cell->ch : (uint32_t)' ') : (uint32_t)' ';
            if (ch < 0x20u) ch = (uint32_t)' ';
            lineBuf[w++] = (ch >= 0x20u && ch < 0x7Fu) ? (char)ch : '?';
        }
        lineBuf[w] = '\0';
        drawFn(x, y + (r - startRow) * lineHeight, lineBuf);
    }
}
