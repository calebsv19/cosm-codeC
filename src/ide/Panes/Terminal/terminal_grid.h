#ifndef TERMINAL_GRID_H
#define TERMINAL_GRID_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint32_t ch;    // codepoint (ASCII for now)
    uint32_t fg;    // packed RGBA or palette index
    uint32_t bg;    // packed RGBA or palette index
    uint8_t attrs;  // bitflags: bold=1<<0, underline=1<<1
} TermCell;

typedef struct {
    TermCell* cells; // linear buffer size rows*cols
    TermCell* primary_cells;
    TermCell* alternate_cells;
    uint8_t using_alternate;
    uint8_t alternate_screen_enabled;
    int rows;
    int cols;
    int viewport_rows;
    int viewport_cols;
    int used_rows; // monotonic visible history depth within this grid
    int cursor_row;
    int cursor_col;
    int primary_used_rows;
    int primary_cursor_row;
    int primary_cursor_col;
    int alternate_used_rows;
    int alternate_cursor_row;
    int alternate_cursor_col;
    int saved_cursor_row;
    int saved_cursor_col;
    uint8_t has_saved_cursor;

    uint32_t cur_fg;
    uint32_t cur_bg;
    uint8_t cur_attrs;

    // Persistent parser state across feed() calls.
    uint8_t parser_state; // 0=text, 1=esc, 2=csi, 3=osc, 4=osc_esc
    char csi_buf[128];
    int csi_len;

    // Stateful UTF-8 decoder across feed() calls.
    uint32_t utf8_codepoint;
    uint8_t utf8_expected;
    uint8_t utf8_seen;

    // Committed scrollback rows (primary buffer only).
    TermCell* scrollback_cells;
    int scrollback_cap_rows;
    int scrollback_count;
    int scrollback_head;
    unsigned long long scrollback_commit_count;
    unsigned long long scrollback_drop_count;
    unsigned long long alt_enter_count;
    unsigned long long alt_exit_count;
    unsigned long long alt_ignored_count;
} TermGrid;

// Lifecycle
void term_grid_init(TermGrid* grid, int rows, int cols);
void term_grid_free(TermGrid* grid);
void term_grid_resize(TermGrid* grid, int rows, int cols);
void term_grid_clear(TermGrid* grid);
void term_grid_clear_line(TermGrid* grid, int row);

// Access
static inline TermCell* term_grid_cell(TermGrid* grid, int r, int c) {
    if (!grid || r < 0 || r >= grid->rows || c < 0 || c >= grid->cols) return NULL;
    return &grid->cells[r * grid->cols + c];
}

// Debug rendering helper (unstyled).
void term_grid_debug_render(TermGrid* grid, int x, int y, int lineHeight, int maxHeight,
                            void (*drawFn)(int, int, const char*));

// Parser entrypoint
void term_emulator_feed(TermGrid* grid, const char* data, size_t len);
void term_grid_set_viewport_size(TermGrid* grid, int rows, int cols);
void term_grid_set_alternate_screen_enabled(TermGrid* grid, int enabled);
void term_grid_set_scrollback_cap(TermGrid* grid, int cap_rows);
int term_grid_scrollback_count(const TermGrid* grid);
const TermCell* term_grid_scrollback_row(const TermGrid* grid, int index);
unsigned long long term_grid_scrollback_commit_count(const TermGrid* grid);
unsigned long long term_grid_scrollback_drop_count(const TermGrid* grid);
unsigned long long term_grid_alt_enter_count(const TermGrid* grid);
unsigned long long term_grid_alt_exit_count(const TermGrid* grid);
unsigned long long term_grid_alt_ignored_count(const TermGrid* grid);

#endif
