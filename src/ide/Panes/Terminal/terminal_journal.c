#include "ide/Panes/Terminal/terminal_journal.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static const uint32_t kJournalDefaultFg = 0xFFFFFFFFu;
static const uint32_t kJournalDefaultBg = 0x000000FFu;

static void journal_fill_row(TermCell* row, int cols) {
    if (!row || cols <= 0) return;
    for (int c = 0; c < cols; ++c) {
        row[c].ch = ' ';
        row[c].fg = kJournalDefaultFg;
        row[c].bg = kJournalDefaultBg;
        row[c].attrs = 0;
    }
}

static bool row_has_text(const TermCell* row, int cols) {
    if (!row || cols <= 0) return false;
    for (int c = 0; c < cols; ++c) {
        uint32_t ch = row[c].ch ? row[c].ch : (uint32_t)' ';
        if (ch != (uint32_t)' ') return true;
    }
    return false;
}

static bool rows_equal(const TermCell* a, int a_cols, const TermCell* b, int b_cols) {
    if (!a || !b || a_cols <= 0 || b_cols <= 0) return false;
    int cols = a_cols < b_cols ? a_cols : b_cols;
    for (int c = 0; c < cols; ++c) {
        if (a[c].ch != b[c].ch ||
            a[c].fg != b[c].fg ||
            a[c].bg != b[c].bg ||
            a[c].attrs != b[c].attrs) {
            return false;
        }
    }
    for (int c = cols; c < a_cols; ++c) {
        uint32_t ch = a[c].ch ? a[c].ch : (uint32_t)' ';
        if (ch != (uint32_t)' ') return false;
    }
    for (int c = cols; c < b_cols; ++c) {
        uint32_t ch = b[c].ch ? b[c].ch : (uint32_t)' ';
        if (ch != (uint32_t)' ') return false;
    }
    return true;
}

static bool row_prefix_matches(const TermCell* prefix, int prefix_cols, const TermCell* row, int row_cols) {
    if (!prefix || !row || prefix_cols <= 0 || row_cols <= 0) return false;
    int cols = prefix_cols < row_cols ? prefix_cols : row_cols;
    bool saw_text = false;
    for (int c = 0; c < cols; ++c) {
        uint32_t ch = prefix[c].ch ? prefix[c].ch : (uint32_t)' ';
        if (ch == (uint32_t)' ') continue;
        saw_text = true;
        if (prefix[c].ch != row[c].ch ||
            prefix[c].fg != row[c].fg ||
            prefix[c].bg != row[c].bg ||
            prefix[c].attrs != row[c].attrs) {
            return false;
        }
    }
    for (int c = cols; c < prefix_cols; ++c) {
        uint32_t ch = prefix[c].ch ? prefix[c].ch : (uint32_t)' ';
        if (ch != (uint32_t)' ') return false;
    }
    return saw_text;
}

static void journal_drop_front(TerminalJournal* journal, int rows) {
    if (!journal || rows <= 0 || journal->row_count <= 0) return;
    if (rows >= journal->row_count) {
        journal->drop_count += (unsigned long long)journal->row_count;
        journal->row_count = 0;
        return;
    }
    int keep = journal->row_count - rows;
    memmove(journal->rows,
            journal->rows + (size_t)rows * (size_t)journal->cols,
            (size_t)keep * (size_t)journal->cols * sizeof(TermCell));
    journal->row_count = keep;
    journal->drop_count += (unsigned long long)rows;
}

static void journal_insert_rows(TerminalJournal* journal,
                                int index,
                                const TermCell* rows,
                                int row_count,
                                int row_cols) {
    if (!journal || !journal->rows || !rows || row_count <= 0 || journal->cols <= 0) return;
    if (index < 0) index = 0;
    if (index > journal->row_count) index = journal->row_count;

    int overflow = journal->row_count + row_count - journal->cap_rows;
    if (overflow > 0) {
        if (overflow >= journal->cap_rows) {
            rows += (size_t)(row_count - journal->cap_rows) * (size_t)row_cols;
            row_count = journal->cap_rows;
            journal->row_count = 0;
            index = 0;
        } else {
            journal_drop_front(journal, overflow);
            index -= overflow;
            if (index < 0) index = 0;
        }
    }

    if (row_count <= 0) return;
    if (journal->row_count > index) {
        memmove(journal->rows + (size_t)(index + row_count) * (size_t)journal->cols,
                journal->rows + (size_t)index * (size_t)journal->cols,
                (size_t)(journal->row_count - index) * (size_t)journal->cols * sizeof(TermCell));
    }

    int copy_cols = row_cols < journal->cols ? row_cols : journal->cols;
    for (int r = 0; r < row_count; ++r) {
        TermCell* dst = journal->rows + (size_t)(index + r) * (size_t)journal->cols;
        journal_fill_row(dst, journal->cols);
        memcpy(dst, rows + (size_t)r * (size_t)row_cols, (size_t)copy_cols * sizeof(TermCell));
    }
    journal->row_count += row_count;
    journal->insert_count += (unsigned long long)row_count;
    if (index + row_count >= journal->row_count) {
        journal->append_count += (unsigned long long)row_count;
    }
}

static void journal_replace_row(TerminalJournal* journal,
                                int index,
                                const TermCell* row,
                                int row_cols) {
    if (!journal || !journal->rows || !row || index < 0 || index >= journal->row_count) return;
    TermCell* dst = journal->rows + (size_t)index * (size_t)journal->cols;
    int copy_cols = row_cols < journal->cols ? row_cols : journal->cols;
    journal_fill_row(dst, journal->cols);
    memcpy(dst, row, (size_t)copy_cols * sizeof(TermCell));
}

void terminal_journal_init(TerminalJournal* journal, int cap_rows, int cols) {
    if (!journal) return;
    memset(journal, 0, sizeof(*journal));
    terminal_journal_configure(journal, cap_rows, cols);
}

void terminal_journal_free(TerminalJournal* journal) {
    if (!journal) return;
    free(journal->rows);
    memset(journal, 0, sizeof(*journal));
}

void terminal_journal_clear(TerminalJournal* journal) {
    if (!journal) return;
    journal->row_count = 0;
}

void terminal_journal_configure(TerminalJournal* journal, int cap_rows, int cols) {
    if (!journal) return;
    if (cap_rows < 0) cap_rows = 0;
    if (cols < 1) cols = 1;
    if (journal->cap_rows == cap_rows && journal->cols == cols && journal->rows) return;

    TermCell* old_rows = journal->rows;
    int old_count = journal->row_count;
    int old_cols = journal->cols;

    TermCell* next = NULL;
    if (cap_rows > 0) {
        next = (TermCell*)malloc((size_t)cap_rows * (size_t)cols * sizeof(TermCell));
        if (!next) return;
        for (int r = 0; r < cap_rows; ++r) {
            journal_fill_row(next + (size_t)r * (size_t)cols, cols);
        }
    }

    int copy_count = old_count;
    if (copy_count > cap_rows) {
        journal->drop_count += (unsigned long long)(copy_count - cap_rows);
        copy_count = cap_rows;
    }
    int start = old_count - copy_count;
    if (next && old_rows && copy_count > 0 && old_cols > 0) {
        int copy_cols = old_cols < cols ? old_cols : cols;
        for (int r = 0; r < copy_count; ++r) {
            const TermCell* src = old_rows + (size_t)(start + r) * (size_t)old_cols;
            TermCell* dst = next + (size_t)r * (size_t)cols;
            memcpy(dst, src, (size_t)copy_cols * sizeof(TermCell));
        }
    }

    free(old_rows);
    journal->rows = next;
    journal->row_count = copy_count;
    journal->cap_rows = cap_rows;
    journal->cols = cols;
}

int terminal_journal_count(const TerminalJournal* journal) {
    return journal ? journal->row_count : 0;
}

const TermCell* terminal_journal_row(const TerminalJournal* journal, int index) {
    if (!journal || !journal->rows || index < 0 || index >= journal->row_count) return NULL;
    return journal->rows + (size_t)index * (size_t)journal->cols;
}

int terminal_journal_find_last_equal_row(const TerminalJournal* journal,
                                         const TermCell* row,
                                         int cols) {
    if (!journal || !row || cols <= 0) return -1;
    for (int i = journal->row_count - 1; i >= 0; --i) {
        const TermCell* candidate = terminal_journal_row(journal, i);
        if (candidate && rows_equal(candidate, journal->cols, row, cols)) return i;
    }
    return -1;
}

void terminal_journal_capture_viewport(TerminalJournal* journal,
                                       const TermGrid* grid,
                                       int start_row,
                                       int row_count,
                                       int cursor_row) {
    if (!journal || !grid || !grid->cells || grid->using_alternate || grid->cols <= 0) return;
    if (row_count <= 0 || journal->cap_rows <= 0) return;
    terminal_journal_configure(journal, journal->cap_rows, grid->cols);
    if (!journal->rows) return;

    if (start_row < 0) start_row = 0;
    if (start_row >= grid->rows) return;
    if (start_row + row_count > grid->rows) row_count = grid->rows - start_row;
    if (row_count <= 0) return;

    int first = -1;
    int last = -1;
    for (int r = 0; r < row_count; ++r) {
        const TermCell* row = grid->cells + (size_t)(start_row + r) * (size_t)grid->cols;
        if (row_has_text(row, grid->cols)) {
            if (first < 0) first = r;
            last = r;
        }
    }
    if (first < 0 || last < first) return;

    const int snap_count = last - first + 1;
    const TermCell* snap = grid->cells + (size_t)(start_row + first) * (size_t)grid->cols;
    journal->capture_count++;

    int best_journal = -1;
    int best_snap = -1;
    int best_len = 0;
    for (int j = 0; j < journal->row_count; ++j) {
        const TermCell* journal_row = terminal_journal_row(journal, j);
        if (!row_has_text(journal_row, journal->cols)) continue;
        for (int s = 0; s < snap_count; ++s) {
            const TermCell* snap_row = snap + (size_t)s * (size_t)grid->cols;
            if (!row_has_text(snap_row, grid->cols)) continue;
            if (!rows_equal(journal_row, journal->cols, snap_row, grid->cols)) continue;
            int len = 0;
            while (j + len < journal->row_count && s + len < snap_count) {
                const TermCell* jr = terminal_journal_row(journal, j + len);
                const TermCell* sr = snap + (size_t)(s + len) * (size_t)grid->cols;
                if (!rows_equal(jr, journal->cols, sr, grid->cols)) break;
                len++;
            }
            if (len > best_len) {
                best_len = len;
                best_journal = j;
                best_snap = s;
            }
        }
    }

    if (best_len <= 0) {
        if (journal->row_count > 0 && cursor_row >= start_row + first && cursor_row <= start_row + last) {
            int cursor_snap = cursor_row - (start_row + first);
            const TermCell* live = snap + (size_t)cursor_snap * (size_t)grid->cols;
            const TermCell* tail = terminal_journal_row(journal, journal->row_count - 1);
            if (tail && row_prefix_matches(tail, journal->cols, live, grid->cols)) {
                journal_replace_row(journal, journal->row_count - 1, live, grid->cols);
                return;
            }
        }
        journal_insert_rows(journal, journal->row_count, snap, snap_count, grid->cols);
        return;
    }

    if (best_snap > 0) {
        journal_insert_rows(journal, best_journal, snap, best_snap, grid->cols);
        best_journal += best_snap;
    }

    int suffix_start = best_snap + best_len;
    if (suffix_start < snap_count) {
        const TermCell* suffix = snap + (size_t)suffix_start * (size_t)grid->cols;
        if (cursor_row >= start_row + first + suffix_start &&
            cursor_row <= start_row + first + snap_count - 1 &&
            best_journal + best_len >= 0) {
            int cursor_snap = cursor_row - (start_row + first);
            int live_offset = cursor_snap - suffix_start;
            const TermCell* live = suffix + (size_t)live_offset * (size_t)grid->cols;
            int replace_index = best_journal + best_len + live_offset;
            const TermCell* replace = terminal_journal_row(journal, replace_index);
            if (replace && row_prefix_matches(replace, journal->cols, live, grid->cols)) {
                journal_replace_row(journal, replace_index, live, grid->cols);
                suffix_start = cursor_snap + 1;
                if (suffix_start >= snap_count) return;
                suffix = snap + (size_t)suffix_start * (size_t)grid->cols;
            }
        }
        journal_insert_rows(journal,
                            best_journal + best_len,
                            suffix,
                            snap_count - suffix_start,
                            grid->cols);
    }
}
