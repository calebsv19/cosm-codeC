#include "terminal_grid.h"
#include "terminal_grid_internal.h"
#include "terminal_grid_sgr_helpers.h"

#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

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

static void clamp_cursor(TermGrid* grid);
static void term_grid_restore_cursor(TermGrid* grid);

void term_grid_set_alternate_screen_enabled(TermGrid* grid, int enabled) {
    if (!grid) return;
    grid->alternate_screen_enabled = enabled ? 1 : 0;
    if (!grid->alternate_screen_enabled && grid->using_alternate) {
        term_grid_debug_log("alt force-exit due to disable");
        term_grid_commit_active_state(grid);
        grid->using_alternate = 0;
        grid->cells = grid->primary_cells;
        term_grid_sync_active_state(grid);
        term_grid_restore_cursor(grid);
        clamp_cursor(grid);
        grid->alt_exit_count++;
    }
}

static void clamp_cursor(TermGrid* grid) {
    if (!grid) return;
    if (grid->cursor_row < 0) grid->cursor_row = 0;
    if (grid->cursor_row >= grid->rows) grid->cursor_row = grid->rows - 1;
    if (grid->cursor_col < 0) grid->cursor_col = 0;
    if (grid->cursor_col >= grid->cols) grid->cursor_col = grid->cols - 1;
    term_grid_commit_active_state(grid);
}

static void clear_cell_to_style(TermGrid* grid, int row, int col) {
    TermCell* cell = term_grid_cell(grid, row, col);
    if (!cell) return;
    cell->ch = ' ';
    cell->fg = grid->cur_fg;
    cell->bg = grid->cur_bg;
    cell->attrs = 0;
}

static void scroll_up_region(TermGrid* grid, int top, int bottom) {
    if (!grid || !grid->cells || grid->rows <= 0) return;
    top = term_grid_clamp_i(top, 0, grid->rows - 1);
    bottom = term_grid_clamp_i(bottom, top, grid->rows - 1);
    if (!grid->using_alternate && top == 0 && bottom == grid->rows - 1 && grid->cols > 0) {
        term_grid_push_scrollback_row(grid, grid->cells);
    }

    if (bottom > top) {
        memmove(grid->cells + (size_t)top * (size_t)grid->cols,
                grid->cells + (size_t)(top + 1) * (size_t)grid->cols,
                (size_t)(bottom - top) * (size_t)grid->cols * sizeof(TermCell));
    }

    for (int c = 0; c < grid->cols; ++c) {
        clear_cell_to_style(grid, bottom, c);
    }

    if (grid->used_rows < bottom + 1) grid->used_rows = bottom + 1;
}

static void scroll_down_region(TermGrid* grid, int top, int bottom) {
    if (!grid || !grid->cells || grid->rows <= 0) return;
    top = term_grid_clamp_i(top, 0, grid->rows - 1);
    bottom = term_grid_clamp_i(bottom, top, grid->rows - 1);

    if (bottom > top) {
        memmove(grid->cells + (size_t)(top + 1) * (size_t)grid->cols,
                grid->cells + (size_t)top * (size_t)grid->cols,
                (size_t)(bottom - top) * (size_t)grid->cols * sizeof(TermCell));
    }

    for (int c = 0; c < grid->cols; ++c) {
        clear_cell_to_style(grid, top, c);
    }

    if (grid->used_rows < bottom + 1) grid->used_rows = bottom + 1;
}

static void insert_blank_chars(TermGrid* grid, int n) {
    if (!grid || !grid->cells || grid->cols <= 0) return;
    if (n < 1) n = 1;
    if (n > grid->cols - grid->cursor_col) n = grid->cols - grid->cursor_col;
    if (n <= 0) return;
    TermCell* row = grid->cells + (size_t)grid->cursor_row * (size_t)grid->cols;
    memmove(row + grid->cursor_col + n,
            row + grid->cursor_col,
            (size_t)(grid->cols - grid->cursor_col - n) * sizeof(TermCell));
    for (int c = 0; c < n; ++c) {
        clear_cell_to_style(grid, grid->cursor_row, grid->cursor_col + c);
    }
}

static void delete_chars(TermGrid* grid, int n) {
    if (!grid || !grid->cells || grid->cols <= 0) return;
    if (n < 1) n = 1;
    if (n > grid->cols - grid->cursor_col) n = grid->cols - grid->cursor_col;
    if (n <= 0) return;
    TermCell* row = grid->cells + (size_t)grid->cursor_row * (size_t)grid->cols;
    memmove(row + grid->cursor_col,
            row + grid->cursor_col + n,
            (size_t)(grid->cols - grid->cursor_col - n) * sizeof(TermCell));
    for (int c = grid->cols - n; c < grid->cols; ++c) {
        clear_cell_to_style(grid, grid->cursor_row, c);
    }
}

static void erase_chars(TermGrid* grid, int n) {
    if (!grid || !grid->cells || grid->cols <= 0) return;
    if (n < 1) n = 1;
    if (n > grid->cols - grid->cursor_col) n = grid->cols - grid->cursor_col;
    for (int c = 0; c < n; ++c) {
        clear_cell_to_style(grid, grid->cursor_row, grid->cursor_col + c);
    }
}

static void insert_blank_lines(TermGrid* grid, int n) {
    if (!grid || !grid->cells || grid->rows <= 0 || grid->cols <= 0) return;
    int top = term_grid_scroll_region_top(grid);
    int bottom = term_grid_scroll_region_bottom(grid);
    if (grid->cursor_row < top || grid->cursor_row > bottom) return;
    if (n < 1) n = 1;
    if (n > bottom - grid->cursor_row + 1) n = bottom - grid->cursor_row + 1;
    if (n <= 0) return;
    if (bottom - grid->cursor_row + 1 > n) {
        memmove(grid->cells + (size_t)(grid->cursor_row + n) * (size_t)grid->cols,
                grid->cells + (size_t)grid->cursor_row * (size_t)grid->cols,
                (size_t)(bottom - grid->cursor_row + 1 - n) * (size_t)grid->cols * sizeof(TermCell));
    }
    for (int r = 0; r < n; ++r) {
        term_grid_clear_line(grid, grid->cursor_row + r);
    }
    if (grid->used_rows < bottom + 1) grid->used_rows = bottom + 1;
}

static void delete_lines(TermGrid* grid, int n) {
    if (!grid || !grid->cells || grid->rows <= 0 || grid->cols <= 0) return;
    int top = term_grid_scroll_region_top(grid);
    int bottom = term_grid_scroll_region_bottom(grid);
    if (grid->cursor_row < top || grid->cursor_row > bottom) return;
    if (n < 1) n = 1;
    if (n > bottom - grid->cursor_row + 1) n = bottom - grid->cursor_row + 1;
    if (n <= 0) return;
    if (bottom - grid->cursor_row + 1 > n) {
        memmove(grid->cells + (size_t)grid->cursor_row * (size_t)grid->cols,
                grid->cells + (size_t)(grid->cursor_row + n) * (size_t)grid->cols,
                (size_t)(bottom - grid->cursor_row + 1 - n) * (size_t)grid->cols * sizeof(TermCell));
    }
    for (int r = bottom - n + 1; r <= bottom; ++r) {
        term_grid_clear_line(grid, r);
    }
    if (grid->used_rows < bottom + 1) grid->used_rows = bottom + 1;
}

static void move_cursor_newline(TermGrid* grid) {
    int previousRow = grid->cursor_row;
    grid->cursor_col = 0;
    grid->cursor_row++;
    if (grid->cursor_row + 1 > grid->used_rows) {
        grid->used_rows = grid->cursor_row + 1;
    }
    int bottom = term_grid_scroll_region_bottom(grid);
    int top = term_grid_scroll_region_top(grid);
    if (previousRow >= top && previousRow <= bottom && grid->cursor_row > bottom) {
        scroll_up_region(grid, top, bottom);
        grid->cursor_row = bottom;
        grid->used_rows = grid->rows;
    } else if (grid->cursor_row >= grid->rows) {
        scroll_up_region(grid, 0, grid->rows - 1);
        grid->cursor_row = grid->rows - 1;
        grid->used_rows = grid->rows;
    }
}

static int codepoint_cell_width(uint32_t cp) {
    if (cp == 0u) return 0;
    if (cp < 0x20u || cp == 0x7Fu) return 0;
    if (cp == 0x200Du) return 0; // zero-width joiner
    if ((cp >= 0x0300u && cp <= 0x036Fu) ||
        (cp >= 0x1AB0u && cp <= 0x1AFFu) ||
        (cp >= 0x1DC0u && cp <= 0x1DFFu) ||
        (cp >= 0x20D0u && cp <= 0x20FFu) ||
        (cp >= 0xFE00u && cp <= 0xFE0Fu) ||
        (cp >= 0xFE20u && cp <= 0xFE2Fu) ||
        (cp >= 0xE0100u && cp <= 0xE01EFu)) {
        return 0;
    }

    if ((cp >= 0x1100u && cp <= 0x115Fu) ||
        cp == 0x2329u || cp == 0x232Au ||
        (cp >= 0x2E80u && cp <= 0xA4CFu && cp != 0x303Fu) ||
        (cp >= 0xAC00u && cp <= 0xD7A3u) ||
        (cp >= 0xF900u && cp <= 0xFAFFu) ||
        (cp >= 0xFE10u && cp <= 0xFE19u) ||
        (cp >= 0xFE30u && cp <= 0xFE6Fu) ||
        (cp >= 0xFF00u && cp <= 0xFF60u) ||
        (cp >= 0xFFE0u && cp <= 0xFFE6u) ||
        (cp >= 0x1F300u && cp <= 0x1FAFFu)) {
        return 2;
    }

    return 1;
}

static void clear_wide_neighbor_if_needed(TermGrid* grid, int row, int col) {
    if (!grid || !grid->cells || row < 0 || row >= grid->rows) return;

    TermCell* current = term_grid_cell(grid, row, col);
    if (current && (current->attrs & ATTR_WIDE_CONTINUATION) && col > 0) {
        clear_cell_to_style(grid, row, col - 1);
    }

    if (col > 0) {
        TermCell* prev = term_grid_cell(grid, row, col - 1);
        if (prev && codepoint_cell_width(prev->ch) == 2) {
            clear_cell_to_style(grid, row, col - 1);
        }
    }

    TermCell* next = term_grid_cell(grid, row, col + 1);
    if (next && (next->attrs & ATTR_WIDE_CONTINUATION)) {
        clear_cell_to_style(grid, row, col + 1);
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

    int width = codepoint_cell_width(cp);
    if (width <= 0) {
        return;
    }
    if (width > 1 && grid->cols < 2) width = 1;
    if (width > 1 && grid->cursor_col >= grid->cols - 1) {
        move_cursor_newline(grid);
    }

    clear_wide_neighbor_if_needed(grid, grid->cursor_row, grid->cursor_col);
    TermCell* cell = term_grid_cell(grid, grid->cursor_row, grid->cursor_col);
    if (cell) {
        cell->ch = cp;
        cell->fg = grid->cur_fg;
        cell->bg = grid->cur_bg;
        cell->attrs = grid->cur_attrs;
    }
    if (width > 1 && grid->cursor_col + 1 < grid->cols) {
        TermCell* continuation = term_grid_cell(grid, grid->cursor_row, grid->cursor_col + 1);
        if (continuation) {
            continuation->ch = ' ';
            continuation->fg = grid->cur_fg;
            continuation->bg = grid->cur_bg;
            continuation->attrs = (uint8_t)(grid->cur_attrs | ATTR_WIDE_CONTINUATION);
        }
    }
    if (grid->cursor_row + 1 > grid->used_rows) {
        grid->used_rows = grid->cursor_row + 1;
    }

    grid->cursor_col += width;
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
        term_grid_reset_style(grid, TERM_DEFAULT_FG, TERM_DEFAULT_BG);
        return;
    }

    for (int i = 0; i < count; ++i) {
        int p = params[i];

        if (p == 0) {
            term_grid_reset_style(grid, TERM_DEFAULT_FG, TERM_DEFAULT_BG);
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
            term_grid_set_sgr_color(grid, 1, term_grid_ansi16_color((unsigned int)(p - 30)));
        } else if (p >= 40 && p <= 47) {
            term_grid_set_sgr_color(grid, 0, term_grid_ansi16_color((unsigned int)(p - 40)));
        } else if (p >= 90 && p <= 97) {
            term_grid_set_sgr_color(grid, 1, term_grid_ansi16_color((unsigned int)(8 + (p - 90))));
        } else if (p >= 100 && p <= 107) {
            term_grid_set_sgr_color(grid, 0, term_grid_ansi16_color((unsigned int)(8 + (p - 100))));
        } else if (p == 38 || p == 48) {
            int is_fg = (p == 38);
            if (i + 1 >= count) continue;

            int mode = params[++i];
            if (mode == 5) {
                if (i + 1 >= count) continue;
                int idx = params[++i];
                term_grid_set_sgr_color(grid, is_fg, term_grid_ansi256_color(idx));
            } else if (mode == 2) {
                if (i + 3 >= count) continue;
                int r = params[++i];
                int g = params[++i];
                int b = params[++i];
                if (r < 0) r = 0; else if (r > 255) r = 255;
                if (g < 0) g = 0; else if (g > 255) g = 255;
                if (b < 0) b = 0; else if (b > 255) b = 255;
                term_grid_set_sgr_color(grid, is_fg, term_grid_pack_rgba((unsigned int)r, (unsigned int)g, (unsigned int)b));
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
        term_grid_commit_active_state(grid);
        if (!grid->alternate_cells) {
            grid->alternate_cells =
                (TermCell*)malloc((size_t)grid->rows * (size_t)grid->cols * sizeof(TermCell));
            if (!grid->alternate_cells) return;
            term_grid_fill_cells(grid->alternate_cells, grid->rows, grid->cols, grid->cur_fg, grid->cur_bg);
            grid->alternate_cursor_row = 0;
            grid->alternate_cursor_col = 0;
            grid->alternate_used_rows = 1;
        } else if (clearOnEnter) {
            term_grid_fill_cells(grid->alternate_cells, grid->rows, grid->cols, grid->cur_fg, grid->cur_bg);
            grid->alternate_cursor_row = 0;
            grid->alternate_cursor_col = 0;
            grid->alternate_used_rows = 1;
        }
        grid->using_alternate = 1;
        grid->cells = grid->alternate_cells;
        term_grid_sync_active_state(grid);
        clamp_cursor(grid);
        return;
    }

    term_grid_commit_active_state(grid);
    grid->using_alternate = 0;
    grid->cells = grid->primary_cells;
    term_grid_sync_active_state(grid);
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
            case 25:
                grid->cursor_visible = setMode ? 1 : 0;
                break;
            case 1000:
            case 1002:
            case 1003:
            case 1006:
                grid->mouse_mode = setMode ? mode : 0;
                break;
            case 2004:
                grid->bracketed_paste = setMode ? 1 : 0;
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
                values[count++] = term_grid_parse_int(p + start, i - start);
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
        case '@': // ICH
            insert_blank_chars(grid, n);
            break;
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
        case 'L': // IL
            insert_blank_lines(grid, n);
            break;
        case 'M': // DL
            delete_lines(grid, n);
            break;
        case 'P': // DCH
            delete_chars(grid, n);
            break;
        case 'X': // ECH
            erase_chars(grid, n);
            break;
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
                    term_grid_clear_active_buffer(grid);
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
        case 'r': { // DECSTBM
            int top = 0;
            int bottom = grid->rows - 1;
            if (count >= 2 && values[0] > 0 && values[1] > 0) {
                top = values[0] - 1;
                bottom = values[1] - 1;
            }
            if (top < 0 || bottom <= top || bottom >= grid->rows) {
                top = 0;
                bottom = grid->rows - 1;
            }
            grid->scroll_top = top;
            grid->scroll_bottom = bottom;
            grid->cursor_row = 0;
            grid->cursor_col = 0;
            break;
        }
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
                } else if (ch == 'M') {
                    int top = term_grid_scroll_region_top(grid);
                    if (grid->cursor_row <= top) {
                        scroll_down_region(grid, top, term_grid_scroll_region_bottom(grid));
                        grid->cursor_row = top;
                    } else {
                        grid->cursor_row--;
                    }
                    grid->parser_state = STATE_TEXT;
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
