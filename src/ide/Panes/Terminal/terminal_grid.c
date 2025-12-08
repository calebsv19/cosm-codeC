#include "terminal_grid.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define ATTR_BOLD      (1 << 0)
#define ATTR_UNDERLINE (1 << 1)

static void grid_alloc(TermGrid* grid, int rows, int cols) {
    if (!grid || rows <= 0 || cols <= 0) return;
    grid->rows = rows;
    grid->cols = cols;
    grid->cells = (TermCell*)malloc((size_t)rows * (size_t)cols * sizeof(TermCell));
    term_grid_clear(grid);
}

void term_grid_init(TermGrid* grid, int rows, int cols) {
    if (!grid) return;
    grid->cells = NULL;
    grid->rows = grid->cols = 0;
    grid->cursor_row = grid->cursor_col = 0;
    grid->cur_fg = 0xFFFFFFFF; // white
    grid->cur_bg = 0x000000FF; // black
    grid->cur_attrs = 0;
    grid_alloc(grid, rows, cols);
}

void term_grid_free(TermGrid* grid) {
    if (!grid) return;
    free(grid->cells);
    grid->cells = NULL;
    grid->rows = grid->cols = 0;
}

void term_grid_clear(TermGrid* grid) {
    if (!grid || !grid->cells) return;
    for (int r = 0; r < grid->rows; ++r) {
        for (int c = 0; c < grid->cols; ++c) {
            TermCell* cell = term_grid_cell(grid, r, c);
            cell->ch = ' ';
            cell->fg = grid->cur_fg;
            cell->bg = grid->cur_bg;
            cell->attrs = 0;
        }
    }
    grid->cursor_row = 0;
    grid->cursor_col = 0;
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
    term_grid_free(grid);
    term_grid_init(grid, rows, cols);
}

static void clamp_cursor(TermGrid* grid) {
    if (!grid) return;
    if (grid->cursor_row < 0) grid->cursor_row = 0;
    if (grid->cursor_row >= grid->rows) grid->cursor_row = grid->rows - 1;
    if (grid->cursor_col < 0) grid->cursor_col = 0;
    if (grid->cursor_col >= grid->cols) grid->cursor_col = grid->cols - 1;
}

static void scroll_up(TermGrid* grid) {
    if (!grid || !grid->cells || grid->rows <= 0) return;
    // Move rows up by 1
    memmove(grid->cells,
            grid->cells + grid->cols,
            (size_t)(grid->rows - 1) * (size_t)grid->cols * sizeof(TermCell));
    // Clear last row
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
    if (grid->cursor_row >= grid->rows) {
        scroll_up(grid);
        grid->cursor_row = grid->rows - 1;
    }
}

static void apply_sgr(TermGrid* grid, const int* params, int count) {
    if (!grid || count == 0) {
        grid->cur_fg = 0xFFFFFFFF;
        grid->cur_bg = 0x000000FF;
        grid->cur_attrs = 0;
        return;
    }

    for (int i = 0; i < count; ++i) {
        int p = params[i];
        if (p == 0) {
            grid->cur_fg = 0xFFFFFFFF;
            grid->cur_bg = 0x000000FF;
            grid->cur_attrs = 0;
        } else if (p == 1) {
            grid->cur_attrs |= ATTR_BOLD;
        } else if (p == 4) {
            grid->cur_attrs |= ATTR_UNDERLINE;
        } else if (p >= 30 && p <= 37) {
            static const uint32_t colors[] = {
                0x000000FF, // black
                0xAA0000FF, // red
                0x00AA00FF, // green
                0xAA5500FF, // yellow
                0x0000AAFF, // blue
                0xAA00AAFF, // magenta
                0x00AAAAFF, // cyan
                0xAAAAAAAA, // white/gray
            };
            grid->cur_fg = colors[p - 30];
        } else if (p >= 40 && p <= 47) {
            static const uint32_t colors[] = {
                0x000000FF, // black
                0xAA0000FF, // red
                0x00AA00FF, // green
                0xAA5500FF, // yellow
                0x0000AAFF, // blue
                0xAA00AAFF, // magenta
                0x00AAAAFF, // cyan
                0xAAAAAAAA, // white/gray
            };
            grid->cur_bg = colors[p - 40];
        }
    }
}

static int parse_int(const char* s, int len) {
    int v = 0;
    for (int i = 0; i < len; ++i) {
        if (!isdigit((unsigned char)s[i])) return v;
        v = v * 10 + (s[i] - '0');
    }
    return v;
}

static void handle_csi(TermGrid* grid, const char* params, int paramLen, char command) {
    int values[8] = {0};
    int count = 0;

    int start = 0;
    for (int i = 0; i <= paramLen; ++i) {
        if (i == paramLen || params[i] == ';') {
            if (count < 8) {
                values[count++] = parse_int(params + start, i - start);
            }
            start = i + 1;
        }
    }
    if (count == 0) {
        values[count++] = 0;
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
        case 'H': // CUP
        case 'f': { // HVP
            int row = (count >= 1 && values[0] > 0) ? values[0] - 1 : 0;
            int col = (count >= 2 && values[1] > 0) ? values[1] - 1 : 0;
            grid->cursor_row = row;
            grid->cursor_col = col;
            clamp_cursor(grid);
            break;
        }
        case 'J': { // ED
            int mode = values[0];
            if (mode == 2) {
                term_grid_clear(grid);
            }
            break;
        }
        case 'K': { // EL
            int mode = values[0];
            if (mode == 0 || mode == 1) {
                // Erase to end (mode 0) or start (mode 1) of line
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
}

void term_emulator_feed(TermGrid* grid, const char* data, size_t len) {
    if (!grid || !grid->cells || !data || len == 0) return;

    enum { STATE_TEXT, STATE_ESC, STATE_CSI } state = STATE_TEXT;
    char csiBuf[64];
    int csiLen = 0;

    for (size_t i = 0; i < len; ++i) {
        unsigned char ch = (unsigned char)data[i];
        switch (state) {
            case STATE_TEXT:
                if (ch == 0x1b) { // ESC
                    state = STATE_ESC;
                } else if (ch == '\n') {
                    move_cursor_newline(grid);
                } else if (ch == '\r') {
                    grid->cursor_col = 0;
                } else if (ch == '\b' || ch == 0x7f) {
                    if (grid->cursor_col > 0) grid->cursor_col--;
                } else if (ch >= 0x20 || ch == '\t') {
                    TermCell* cell = term_grid_cell(grid, grid->cursor_row, grid->cursor_col);
                    if (cell) {
                        cell->ch = ch;
                        cell->fg = grid->cur_fg;
                        cell->bg = grid->cur_bg;
                        cell->attrs = grid->cur_attrs;
                    }
                    grid->cursor_col++;
                    if (grid->cursor_col >= grid->cols) {
                        move_cursor_newline(grid);
                    }
                }
                break;
            case STATE_ESC:
                if (ch == '[') {
                    state = STATE_CSI;
                    csiLen = 0;
                } else {
                    state = STATE_TEXT;
                }
                break;
            case STATE_CSI:
                if (ch >= 0x40 && ch <= 0x7E) {
                    handle_csi(grid, csiBuf, csiLen, (char)ch);
                    state = STATE_TEXT;
                } else {
                    if (csiLen < (int)sizeof(csiBuf) - 1) {
                        csiBuf[csiLen++] = (char)ch;
                    }
                }
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
            char ch = cell ? (char)(cell->ch ? cell->ch : ' ') : ' ';
            lineBuf[w++] = (ch >= 0x20 && ch < 0x7F) ? ch : ' ';
        }
        lineBuf[w] = '\0';
        drawFn(x, y + (r - startRow) * lineHeight, lineBuf);
    }
}
