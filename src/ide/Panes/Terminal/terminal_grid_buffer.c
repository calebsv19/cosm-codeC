#include "terminal_grid_internal.h"

#include <stdlib.h>
#include <string.h>

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

void term_grid_sync_active_state(TermGrid* grid) {
    if (!grid) return;
    int* used = active_used_rows_ptr(grid);
    int* row = active_cursor_row_ptr(grid);
    int* col = active_cursor_col_ptr(grid);
    if (used) grid->used_rows = *used;
    if (row) grid->cursor_row = *row;
    if (col) grid->cursor_col = *col;
}

int term_grid_scroll_region_top(const TermGrid* grid) {
    if (!grid || grid->rows <= 0) return 0;
    if (grid->scroll_top < 0 || grid->scroll_top >= grid->rows) return 0;
    return grid->scroll_top;
}

int term_grid_scroll_region_bottom(const TermGrid* grid) {
    if (!grid || grid->rows <= 0) return 0;
    if (grid->scroll_bottom < 0 || grid->scroll_bottom >= grid->rows) return grid->rows - 1;
    if (grid->scroll_bottom < term_grid_scroll_region_top(grid)) return grid->rows - 1;
    return grid->scroll_bottom;
}

void term_grid_commit_active_state(TermGrid* grid) {
    if (!grid) return;
    int* used = active_used_rows_ptr(grid);
    int* row = active_cursor_row_ptr(grid);
    int* col = active_cursor_col_ptr(grid);
    if (used) *used = grid->used_rows;
    if (row) *row = grid->cursor_row;
    if (col) *col = grid->cursor_col;
}

void term_grid_fill_cells(TermCell* cells, int rows, int cols, uint32_t fg, uint32_t bg) {
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

int term_grid_clamp_i(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void resize_preserve_tail(const TermCell* oldCells, int oldRows, int oldCols,
                                 TermCell* newCells, int newRows, int newCols,
                                 int* io_used_rows, int* io_cursor_row, int* io_cursor_col) {
    if (!oldCells || !newCells || oldRows <= 0 || oldCols <= 0 || newRows <= 0 || newCols <= 0) return;
    if (!io_used_rows || !io_cursor_row || !io_cursor_col) return;

    int oldUsed = term_grid_clamp_i(*io_used_rows, 1, oldRows);
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
    *io_cursor_row = term_grid_clamp_i(mappedRow, 0, newRows - 1);
    *io_cursor_col = term_grid_clamp_i(*io_cursor_col, 0, newCols - 1);
    *io_used_rows = term_grid_clamp_i(dstStart + copyRows, 1, newRows);
}

void term_grid_clear_scrollback(TermGrid* grid) {
    if (!grid) return;
    grid->scrollback_count = 0;
    grid->scrollback_head = 0;
    grid->scrollback_commit_count = 0;
    grid->scrollback_drop_count = 0;
    grid->alt_enter_count = 0;
    grid->alt_exit_count = 0;
    grid->alt_ignored_count = 0;
}

void term_grid_push_scrollback_row(TermGrid* grid, const TermCell* row_src) {
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

void term_grid_clear_active_buffer(TermGrid* grid) {
    if (!grid || !grid->cells || grid->rows <= 0 || grid->cols <= 0) return;
    term_grid_fill_cells(grid->cells, grid->rows, grid->cols, grid->cur_fg, grid->cur_bg);
    grid->cursor_row = 0;
    grid->cursor_col = 0;
    grid->used_rows = 1;
    term_grid_commit_active_state(grid);
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
    grid->scroll_top = 0;
    grid->scroll_bottom = rows - 1;
    grid->cursor_visible = 1;
    grid->bracketed_paste = 0;
    grid->mouse_mode = 0;
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
    term_grid_fill_cells(grid->cells, grid->rows, grid->cols, grid->cur_fg, grid->cur_bg);
    grid->cursor_row = 0;
    grid->cursor_col = 0;
    grid->used_rows = 1;
    term_grid_clear_scrollback(grid);
    term_grid_commit_active_state(grid);
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
    term_grid_fill_cells(newPrimary, rows, cols, grid->cur_fg, grid->cur_bg);

    TermCell* newAlternate = NULL;
    if (oldAlternate) {
        newAlternate = (TermCell*)malloc((size_t)rows * (size_t)cols * sizeof(TermCell));
        if (!newAlternate) {
            free(newPrimary);
            return;
        }
        term_grid_fill_cells(newAlternate, rows, cols, grid->cur_fg, grid->cur_bg);
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
    if (grid->scroll_top < 0 || grid->scroll_top >= rows ||
        grid->scroll_bottom < grid->scroll_top || grid->scroll_bottom >= rows) {
        grid->scroll_top = 0;
        grid->scroll_bottom = rows - 1;
    }
    if (grid->viewport_rows < 1 || grid->viewport_rows > rows) grid->viewport_rows = rows;
    if (grid->viewport_cols < 1 || grid->viewport_cols > cols) grid->viewport_cols = cols;
    grid->cells = grid->using_alternate ? grid->alternate_cells : grid->primary_cells;
    if (!grid->cells) grid->cells = grid->primary_cells;
    if (grid->scrollback_cells && oldCols != cols && grid->scrollback_cap_rows > 0) {
        TermCell* oldRing = grid->scrollback_cells;
        TermCell* newRing = (TermCell*)malloc((size_t)grid->scrollback_cap_rows * (size_t)cols * sizeof(TermCell));
        if (newRing) {
            term_grid_fill_cells(newRing, grid->scrollback_cap_rows, cols, TERM_DEFAULT_FG, TERM_DEFAULT_BG);
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

    term_grid_sync_active_state(grid);
    if (grid->cursor_row < 0) grid->cursor_row = 0;
    if (grid->cursor_row >= grid->rows) grid->cursor_row = grid->rows - 1;
    if (grid->cursor_col < 0) grid->cursor_col = 0;
    if (grid->cursor_col >= grid->cols) grid->cursor_col = grid->cols - 1;
    if (grid->used_rows < 1) grid->used_rows = 1;
    if (grid->used_rows > grid->rows) grid->used_rows = grid->rows;
    if (grid->cursor_row + 1 > grid->used_rows) grid->used_rows = grid->cursor_row + 1;
    term_grid_commit_active_state(grid);
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
