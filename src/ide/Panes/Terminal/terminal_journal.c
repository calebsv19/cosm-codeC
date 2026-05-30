#include "ide/Panes/Terminal/terminal_journal.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const uint32_t kJournalDefaultFg = 0xFFFFFFFFu;
static const uint32_t kJournalDefaultBg = 0x000000FFu;
static const int kJournalPendingCapRows = 128;

typedef enum CodexRowClass {
    CODEX_ROW_EMPTY,
    CODEX_ROW_CHROME_LIVE,
    CODEX_ROW_MODAL_LIVE,
    CODEX_ROW_PROMPT_LIVE,
    CODEX_ROW_TRANSCRIPT
} CodexRowClass;

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

static bool row_contains_ascii(const TermCell* row, int cols, const char* needle) {
    if (!row || cols <= 0 || !needle || !needle[0]) return false;
    int n = (int)strlen(needle);
    if (n <= 0 || n > cols) return false;
    for (int start = 0; start <= cols - n; ++start) {
        bool match = true;
        for (int i = 0; i < n; ++i) {
            uint32_t ch = row[start + i].ch ? row[start + i].ch : (uint32_t)' ';
            if (ch != (unsigned char)needle[i]) {
                match = false;
                break;
            }
        }
        if (match) return true;
    }
    return false;
}

static bool row_is_shell_command_candidate(const TermCell* row, int cols) {
    if (!row || cols <= 0) return false;
    if (row_contains_ascii(row, cols, " % ") ||
        row_contains_ascii(row, cols, " $ ") ||
        row_contains_ascii(row, cols, " # ")) {
        return true;
    }
    return false;
}

static int row_first_text_col(const TermCell* row, int cols) {
    if (!row || cols <= 0) return -1;
    for (int c = 0; c < cols; ++c) {
        uint32_t ch = row[c].ch ? row[c].ch : (uint32_t)' ';
        if (ch != (uint32_t)' ') return c;
    }
    return -1;
}

static bool row_is_codex_prompt(const TermCell* row, int cols) {
    int first = row_first_text_col(row, cols);
    if (first < 0) return false;
    uint32_t ch = row[first].ch;
    if (ch == 0x203Au || ch == (uint32_t)'>') return true;
    return first + 2 < cols &&
           row[first].ch == 0xE2u &&
           row[first + 1].ch == 0x80u &&
           row[first + 2].ch == 0xBAu;
}

static bool row_is_codex_prompt_with_input(const TermCell* row, int cols) {
    if (!row_is_codex_prompt(row, cols)) return false;
    int first = row_first_text_col(row, cols);
    if (first < 0) return false;
    int start = first + 1;
    if (first + 2 < cols &&
        row[first].ch == 0xE2u &&
        row[first + 1].ch == 0x80u &&
        row[first + 2].ch == 0xBAu) {
        start = first + 3;
    }
    for (int c = start; c < cols; ++c) {
        uint32_t ch = row[c].ch ? row[c].ch : (uint32_t)' ';
        if (ch != (uint32_t)' ') return true;
    }
    return false;
}

static bool row_is_codex_prompt_control_input(const TermCell* row, int cols) {
    if (!row_is_codex_prompt_with_input(row, cols)) return false;
    if (row_contains_ascii(row, cols, "Keep current model") ||
        row_contains_ascii(row, cols, "Upgrade to") ||
        row_contains_ascii(row, cols, "current model")) {
        return true;
    }

    int first = row_first_text_col(row, cols);
    if (first < 0) return false;
    int start = first + 1;
    if (first + 2 < cols &&
        row[first].ch == 0xE2u &&
        row[first + 1].ch == 0x80u &&
        row[first + 2].ch == 0xBAu) {
        start = first + 3;
    }
    while (start < cols) {
        uint32_t ch = row[start].ch ? row[start].ch : (uint32_t)' ';
        if (ch != (uint32_t)' ') break;
        start++;
    }
    if (start >= cols) return false;
    uint32_t ch = row[start].ch;
    return ch == (uint32_t)'/' || (ch >= (uint32_t)'0' && ch <= (uint32_t)'9');
}

static bool row_first_text_is_ascii(const TermCell* row, int cols, char ch) {
    int first = row_first_text_col(row, cols);
    return first >= 0 && row[first].ch == (uint32_t)(unsigned char)ch;
}

static bool row_first_text_is_digit(const TermCell* row, int cols) {
    int first = row_first_text_col(row, cols);
    if (first < 0) return false;
    uint32_t ch = row[first].ch;
    return ch >= (uint32_t)'0' && ch <= (uint32_t)'9';
}

static bool row_is_box_or_border(const TermCell* row, int cols) {
    int first = row_first_text_col(row, cols);
    if (first < 0) return false;
    uint32_t ch = row[first].ch;
    if (ch == 0xE2u && first + 1 < cols) {
        uint32_t next = row[first + 1].ch;
        return next == 0x94u || next == 0x95u;
    }
    return ch == 0x2500u ||
           ch == 0x2502u ||
           ch == 0x256Du ||
           ch == 0x256Eu ||
           ch == 0x256Fu ||
           ch == 0x2570u;
}

static CodexRowClass codex_row_classify(const TermCell* row, int cols) {
    if (!row || cols <= 0 || !row_has_text(row, cols)) return CODEX_ROW_EMPTY;
    if (row_contains_ascii(row, cols, "Select Model and Effort") ||
        row_contains_ascii(row, cols, "Select Reasoning Level") ||
        row_contains_ascii(row, cols, "Access legacy models") ||
        row_contains_ascii(row, cols, "Press enter to confirm") ||
        row_contains_ascii(row, cols, "esc to go back") ||
        row_contains_ascii(row, cols, "Keep current model") ||
        row_contains_ascii(row, cols, "Upgrade to") ||
        row_contains_ascii(row, cols, "choose what model") ||
        row_contains_ascii(row, cols, "choose what Codex") ||
        row_contains_ascii(row, cols, "include current selection") ||
        row_contains_ascii(row, cols, "choose what Codex is allowed") ||
        row_contains_ascii(row, cols, "remap TUI shortcuts") ||
        row_contains_ascii(row, cols, "toggle Vim mode") ||
        row_contains_ascii(row, cols, "toggle experimental") ||
        row_contains_ascii(row, cols, "approve one retry") ||
        row_contains_ascii(row, cols, "configure memory") ||
        row_contains_ascii(row, cols, "mention a file") ||
        row_contains_ascii(row, cols, "list configured MCP") ||
        row_contains_ascii(row, cols, "Use /fast to enable") ||
        row_contains_ascii(row, cols, "fastest inference") ||
        row_contains_ascii(row, cols, "Low               Fast responses") ||
        row_contains_ascii(row, cols, "Medium            ") ||
        row_contains_ascii(row, cols, "High              Greater") ||
        row_contains_ascii(row, cols, "Extra high") ||
        row_first_text_is_ascii(row, cols, '/') ||
        (row_first_text_is_digit(row, cols) &&
         (row_contains_ascii(row, cols, ". Low") ||
          row_contains_ascii(row, cols, ". Medium") ||
          row_contains_ascii(row, cols, ". High") ||
          row_contains_ascii(row, cols, ". Extra high") ||
          row_contains_ascii(row, cols, ". gpt-")))) {
        return CODEX_ROW_MODAL_LIVE;
    }
    if (row_contains_ascii(row, cols, "OpenAI Codex") ||
        row_contains_ascii(row, cols, "Update available") ||
        row_contains_ascii(row, cols, "Run brew upgrade") ||
        row_contains_ascii(row, cols, "release notes") ||
        row_contains_ascii(row, cols, "github.com/openai/codex/releases") ||
        row_contains_ascii(row, cols, "Tip: Try the Codex App") ||
        row_contains_ascii(row, cols, "directory: ~/") ||
        row_contains_ascii(row, cols, "MCP client for `") ||
        row_contains_ascii(row, cols, "MCP startup incomplete") ||
        row_contains_ascii(row, cols, "MCP startup failed") ||
        row_contains_ascii(row, cols, "closed: initialize response") ||
        row_contains_ascii(row, cols, "Model changed to ") ||
        row_contains_ascii(row, cols, "model:     loading") ||
        row_contains_ascii(row, cols, "/model to change") ||
        row_contains_ascii(row, cols, "Booting MCP server") ||
        row_contains_ascii(row, cols, "esc to interrupt") ||
        row_contains_ascii(row, cols, "Working (") ||
        row_contains_ascii(row, cols, "Thinking (") ||
        row_is_box_or_border(row, cols)) {
        return CODEX_ROW_CHROME_LIVE;
    }
    if (row_contains_ascii(row, cols, "gpt-") &&
        (row_contains_ascii(row, cols, "~/Desktop/CodeWork") ||
         row_contains_ascii(row, cols, "medium") ||
         row_contains_ascii(row, cols, "high"))) {
        return CODEX_ROW_CHROME_LIVE;
    }
    if (row_is_codex_prompt(row, cols)) {
        return CODEX_ROW_PROMPT_LIVE;
    }
    return CODEX_ROW_TRANSCRIPT;
}

static bool row_is_codex_live_control(const TermCell* row, int cols) {
    CodexRowClass cls = codex_row_classify(row, cols);
    return cls == CODEX_ROW_EMPTY ||
           cls == CODEX_ROW_CHROME_LIVE ||
           cls == CODEX_ROW_MODAL_LIVE ||
           cls == CODEX_ROW_PROMPT_LIVE;
}

static bool rows_contain_text(const TermCell* rows, int row_count, int cols, const char* text) {
    if (!rows || row_count <= 0 || cols <= 0 || !text || !text[0]) return false;
    for (int r = 0; r < row_count; ++r) {
        if (row_contains_ascii(rows + (size_t)r * (size_t)cols, cols, text)) return true;
    }
    return false;
}

static bool rows_contain_class(const TermCell* rows, int row_count, int cols, CodexRowClass cls) {
    if (!rows || row_count <= 0 || cols <= 0) return false;
    for (int r = 0; r < row_count; ++r) {
        if (codex_row_classify(rows + (size_t)r * (size_t)cols, cols) == cls) return true;
    }
    return false;
}

static bool rows_look_like_codex_screen(const TermCell* rows, int row_count, int cols) {
    if (!rows || row_count <= 0 || cols <= 0) return false;
    return rows_contain_text(rows, row_count, cols, "OpenAI Codex") ||
           rows_contain_text(rows, row_count, cols, "gpt-") ||
           rows_contain_text(rows, row_count, cols, "esc to interrupt") ||
           rows_contain_text(rows, row_count, cols, "Working (") ||
           rows_contain_text(rows, row_count, cols, "Thinking (") ||
           rows_contain_text(rows, row_count, cols, "Select Model and Effort") ||
           rows_contain_text(rows, row_count, cols, "Select Reasoning Level");
}

static bool rows_contain_equal(const TermCell* rows,
                               int row_count,
                               int cols,
                               const TermCell* row,
                               int row_cols) {
    if (!rows || row_count <= 0 || cols <= 0 || !row || row_cols <= 0) return false;
    for (int r = 0; r < row_count; ++r) {
        if (rows_equal(rows + (size_t)r * (size_t)cols, cols, row, row_cols)) return true;
    }
    return false;
}

static bool journal_trace_enabled(void) {
    static int cached = -1;
    if (cached < 0) {
        const char* value = getenv("IDE_TERMINAL_JOURNAL_TRACE");
        cached = (value && value[0] && strcmp(value, "0") != 0) ? 1 : 0;
    }
    return cached != 0;
}

static void journal_trace_row(const char* event, const TermCell* row, int cols) {
    if (!journal_trace_enabled() || !event || !row || cols <= 0) return;
    fprintf(stderr, "[terminal_journal] %s: ", event);
    for (int c = 0; c < cols; ++c) {
        uint32_t ch = row[c].ch ? row[c].ch : (uint32_t)' ';
        if (ch == (uint32_t)' ') {
            fputc(' ', stderr);
        } else if (ch >= 32u && ch <= 126u) {
            fputc((int)ch, stderr);
        } else {
            fprintf(stderr, "\\u%04x", (unsigned int)ch);
        }
    }
    fputc('\n', stderr);
}

static void journal_drop_front(TerminalJournal* journal, int rows) {
    if (!journal || rows <= 0 || journal->row_count <= 0) return;
    if (rows >= journal->row_count) {
        journal->drop_count += (unsigned long long)journal->row_count;
        journal->row_count = 0;
        journal->durable_count = 0;
        journal->live_count = 0;
        return;
    }
    int keep = journal->row_count - rows;
    memmove(journal->rows,
            journal->rows + (size_t)rows * (size_t)journal->cols,
            (size_t)keep * (size_t)journal->cols * sizeof(TermCell));
    journal->row_count = keep;
    if (rows >= journal->durable_count) {
        int live_drop = rows - journal->durable_count;
        journal->durable_count = 0;
        journal->live_count -= live_drop;
        if (journal->live_count < 0) journal->live_count = 0;
    } else {
        journal->durable_count -= rows;
    }
    journal->drop_count += (unsigned long long)rows;
}

static int journal_find_durable_equal(const TerminalJournal* journal,
                                      const TermCell* row,
                                      int row_cols) {
    if (!journal || !row || row_cols <= 0) return -1;
    int limit = journal->durable_count;
    if (limit > journal->row_count) limit = journal->row_count;
    for (int i = limit - 1; i >= 0; --i) {
        const TermCell* candidate = terminal_journal_row(journal, i);
        if (candidate && rows_equal(candidate, journal->cols, row, row_cols)) return i;
    }
    return -1;
}

static void journal_insert_one_raw(TerminalJournal* journal,
                                   int index,
                                   const TermCell* row,
                                   int row_cols) {
    if (!journal || !journal->rows || !row || journal->cols <= 0) return;
    if (index < 0) index = 0;
    if (index > journal->row_count) index = journal->row_count;

    int overflow = journal->row_count + 1 - journal->cap_rows;
    if (overflow > 0) {
        if (overflow >= journal->cap_rows) {
            journal->row_count = 0;
            index = 0;
        } else {
            journal_drop_front(journal, overflow);
            index -= overflow;
            if (index < 0) index = 0;
        }
    }

    if (journal->row_count > index) {
        memmove(journal->rows + (size_t)(index + 1) * (size_t)journal->cols,
                journal->rows + (size_t)index * (size_t)journal->cols,
                (size_t)(journal->row_count - index) * (size_t)journal->cols * sizeof(TermCell));
    }

    int copy_cols = row_cols < journal->cols ? row_cols : journal->cols;
    TermCell* dst = journal->rows + (size_t)index * (size_t)journal->cols;
    journal_fill_row(dst, journal->cols);
    memcpy(dst, row, (size_t)copy_cols * sizeof(TermCell));
    journal->row_count++;
    journal->insert_count++;
    if (index + 1 >= journal->row_count) {
        journal->append_count++;
    }
}

static void journal_append_durable_row(TerminalJournal* journal, const TermCell* row, int row_cols) {
    if (!journal || !row) return;
    int index = journal->durable_count;
    journal_insert_one_raw(journal, index, row, row_cols);
    journal->durable_count = index + 1;
    if (journal->durable_count > journal->row_count) journal->durable_count = journal->row_count;
    journal_trace_row("durable", row, row_cols);
}

static void journal_replace_live_tail(TerminalJournal* journal,
                                      const TermCell* rows,
                                      int row_count,
                                      int row_cols) {
    if (!journal) return;
    if (journal->durable_count < 0) journal->durable_count = 0;
    if (journal->durable_count > journal->row_count) journal->durable_count = journal->row_count;
    journal->row_count = journal->durable_count;
    journal->live_count = 0;
    for (int r = 0; rows && r < row_count; ++r) {
        journal_insert_one_raw(journal,
                               journal->row_count,
                               rows + (size_t)r * (size_t)row_cols,
                               row_cols);
        journal->live_count++;
    }
}

static void journal_sync_scrollback_rows(TerminalJournal* journal, const TermGrid* grid) {
    if (!journal || !grid || grid->using_alternate) return;
    int scrollback_count = term_grid_scrollback_count(grid);
    if (scrollback_count < 0) scrollback_count = 0;
    if (journal->scrollback_rows_seen > scrollback_count) {
        journal->scrollback_rows_seen = 0;
    }
    for (int i = journal->scrollback_rows_seen; i < scrollback_count; ++i) {
        const TermCell* row = term_grid_scrollback_row(grid, i);
        if (row) journal_append_durable_row(journal, row, grid->cols);
    }
    journal->scrollback_rows_seen = scrollback_count;
}

static void journal_commit_submitted_command_if_needed(TerminalJournal* journal,
                                                       const TermCell* snap,
                                                       int snap_count,
                                                       int snap_cols) {
    if (!journal || !journal->rows || journal->live_count <= 0 || !snap || snap_count <= 0) return;
    if (!rows_contain_text(snap, snap_count, snap_cols, "OpenAI Codex")) return;

    int live_start = journal->row_count - journal->live_count;
    if (live_start < 0) live_start = 0;
    for (int i = journal->row_count - 1; i >= live_start; --i) {
        const TermCell* live = terminal_journal_row(journal, i);
        if (!live || !row_is_shell_command_candidate(live, journal->cols)) continue;
        if (rows_contain_equal(snap, snap_count, snap_cols, live, journal->cols)) continue;
        if (journal_find_durable_equal(journal, live, journal->cols) >= 0) continue;
        TermCell* copy = (TermCell*)malloc((size_t)journal->cols * sizeof(TermCell));
        if (!copy) return;
        memcpy(copy, live, (size_t)journal->cols * sizeof(TermCell));
        journal_append_durable_row(journal, copy, journal->cols);
        free(copy);
        return;
    }
}

static void journal_commit_submitted_codex_prompt_if_needed(TerminalJournal* journal,
                                                            const TermCell* snap,
                                                            int snap_count,
                                                            int snap_cols) {
    if (!journal || !journal->rows || journal->live_count <= 0 || !snap || snap_count <= 0) return;
    if (rows_contain_text(snap, snap_count, snap_cols, "Select Model and Effort") ||
        rows_contain_text(snap, snap_count, snap_cols, "Select Reasoning Level")) {
        return;
    }
    if (!rows_contain_text(snap, snap_count, snap_cols, "Working (") &&
        !rows_contain_text(snap, snap_count, snap_cols, "Thinking (") &&
        !rows_contain_text(snap, snap_count, snap_cols, "• ") &&
        !rows_contain_text(snap, snap_count, snap_cols, "Model changed to ")) {
        return;
    }

    int live_start = journal->row_count - journal->live_count;
    if (live_start < 0) live_start = 0;
    const TermCell* live_rows = journal->rows + (size_t)live_start * (size_t)journal->cols;
    if (rows_contain_class(live_rows, journal->live_count, journal->cols, CODEX_ROW_MODAL_LIVE)) {
        return;
    }
    for (int i = journal->row_count - 1; i >= live_start; --i) {
        const TermCell* live = terminal_journal_row(journal, i);
        if (!live || !row_is_codex_prompt_with_input(live, journal->cols)) continue;
        if (row_is_codex_prompt_control_input(live, journal->cols)) continue;
        if (rows_contain_equal(snap, snap_count, snap_cols, live, journal->cols)) continue;
        if (journal_find_durable_equal(journal, live, journal->cols) >= 0) continue;
        TermCell* copy = (TermCell*)malloc((size_t)journal->cols * sizeof(TermCell));
        if (!copy) return;
        memcpy(copy, live, (size_t)journal->cols * sizeof(TermCell));
        journal_append_durable_row(journal, copy, journal->cols);
        free(copy);
        return;
    }
}

static void journal_commit_disappeared_codex_rows(TerminalJournal* journal,
                                                  const TermCell* snap,
                                                  int snap_count,
                                                  int snap_cols) {
    if (!journal || !journal->rows || journal->live_count <= 0 || !snap || snap_count <= 0) return;
    int live_start = journal->row_count - journal->live_count;
    if (live_start < 0) return;
    const TermCell* live_rows = journal->rows + (size_t)live_start * (size_t)journal->cols;
    if (!rows_look_like_codex_screen(live_rows, journal->live_count, journal->cols)) return;

    int live_count = journal->live_count;
    int cols = journal->cols;
    TermCell* live_copy = (TermCell*)malloc((size_t)live_count * (size_t)cols * sizeof(TermCell));
    if (!live_copy) return;
    memcpy(live_copy, live_rows, (size_t)live_count * (size_t)cols * sizeof(TermCell));

    for (int i = 0; i < live_count; ++i) {
        const TermCell* live = live_copy + (size_t)i * (size_t)cols;
        if (row_is_codex_live_control(live, cols)) continue;
        if (rows_contain_equal(snap, snap_count, snap_cols, live, cols)) continue;
        if (journal_find_durable_equal(journal, live, cols) >= 0) continue;
        journal_append_durable_row(journal, live, cols);
    }
    free(live_copy);
}

static int journal_pending_find_equal(const TerminalJournal* journal, const TermCell* row, int row_cols) {
    if (!journal || !journal->pending_rows || !row || row_cols <= 0) return -1;
    for (int i = journal->pending_count - 1; i >= 0; --i) {
        const TermCell* candidate = journal->pending_rows + (size_t)i * (size_t)journal->cols;
        if (rows_equal(candidate, journal->cols, row, row_cols)) return i;
    }
    return -1;
}

static bool journal_pending_configure(TerminalJournal* journal, int cols) {
    if (!journal || cols <= 0) return false;
    if (journal->pending_rows && journal->pending_cap_rows == kJournalPendingCapRows && journal->cols == cols) {
        return true;
    }

    TermCell* old_rows = journal->pending_rows;
    int old_count = journal->pending_count;
    int old_cols = journal->cols;

    TermCell* next = (TermCell*)malloc((size_t)kJournalPendingCapRows * (size_t)cols * sizeof(TermCell));
    if (!next) return false;
    for (int r = 0; r < kJournalPendingCapRows; ++r) {
        journal_fill_row(next + (size_t)r * (size_t)cols, cols);
    }

    int copy_count = old_count;
    if (copy_count > kJournalPendingCapRows) copy_count = kJournalPendingCapRows;
    if (old_rows && old_cols > 0) {
        int copy_cols = old_cols < cols ? old_cols : cols;
        for (int r = 0; r < copy_count; ++r) {
            const TermCell* src = old_rows + (size_t)r * (size_t)old_cols;
            TermCell* dst = next + (size_t)r * (size_t)cols;
            memcpy(dst, src, (size_t)copy_cols * sizeof(TermCell));
        }
    }

    free(old_rows);
    journal->pending_rows = next;
    journal->pending_count = copy_count;
    journal->pending_cap_rows = kJournalPendingCapRows;
    return true;
}

static void journal_pending_add(TerminalJournal* journal, const TermCell* row, int row_cols) {
    if (!journal || !row || row_cols <= 0) return;
    if (codex_row_classify(row, row_cols) != CODEX_ROW_TRANSCRIPT) return;
    if (journal_find_durable_equal(journal, row, row_cols) >= 0) return;
    if (!journal_pending_configure(journal, journal->cols)) return;
    if (journal_pending_find_equal(journal, row, row_cols) >= 0) return;

    if (journal->pending_count >= journal->pending_cap_rows) {
        memmove(journal->pending_rows,
                journal->pending_rows + (size_t)journal->cols,
                (size_t)(journal->pending_cap_rows - 1) * (size_t)journal->cols * sizeof(TermCell));
        journal->pending_count = journal->pending_cap_rows - 1;
    }

    TermCell* dst = journal->pending_rows + (size_t)journal->pending_count * (size_t)journal->cols;
    int copy_cols = row_cols < journal->cols ? row_cols : journal->cols;
    journal_fill_row(dst, journal->cols);
    memcpy(dst, row, (size_t)copy_cols * sizeof(TermCell));
    journal->pending_count++;
    journal_trace_row("pending", row, row_cols);
}

static void journal_pending_remove_at(TerminalJournal* journal, int index) {
    if (!journal || !journal->pending_rows || index < 0 || index >= journal->pending_count) return;
    if (index + 1 < journal->pending_count) {
        memmove(journal->pending_rows + (size_t)index * (size_t)journal->cols,
                journal->pending_rows + (size_t)(index + 1) * (size_t)journal->cols,
                (size_t)(journal->pending_count - index - 1) * (size_t)journal->cols * sizeof(TermCell));
    }
    journal->pending_count--;
}

static void journal_commit_pending_codex_rows(TerminalJournal* journal,
                                              const TermCell* snap,
                                              int snap_count,
                                              int snap_cols,
                                              bool commit_all) {
    if (!journal || !journal->pending_rows || journal->pending_count <= 0) return;
    for (int i = 0; i < journal->pending_count;) {
        const TermCell* pending = journal->pending_rows + (size_t)i * (size_t)journal->cols;
        bool still_visible = !commit_all && rows_contain_equal(snap, snap_count, snap_cols, pending, journal->cols);
        if (still_visible) {
            i++;
            continue;
        }
        if (journal_find_durable_equal(journal, pending, journal->cols) < 0) {
            journal_append_durable_row(journal, pending, journal->cols);
        }
        journal_pending_remove_at(journal, i);
    }
}

static void journal_track_codex_pending_rows(TerminalJournal* journal,
                                             const TermCell* snap,
                                             int snap_count,
                                             int snap_cols) {
    if (!journal || !snap || snap_count <= 0 || snap_cols <= 0) return;
    if (!rows_look_like_codex_screen(snap, snap_count, snap_cols)) return;
    for (int r = 0; r < snap_count; ++r) {
        const TermCell* row = snap + (size_t)r * (size_t)snap_cols;
        journal_pending_add(journal, row, snap_cols);
    }
}

void terminal_journal_init(TerminalJournal* journal, int cap_rows, int cols) {
    if (!journal) return;
    memset(journal, 0, sizeof(*journal));
    terminal_journal_configure(journal, cap_rows, cols);
}

void terminal_journal_free(TerminalJournal* journal) {
    if (!journal) return;
    free(journal->rows);
    free(journal->pending_rows);
    memset(journal, 0, sizeof(*journal));
}

void terminal_journal_clear(TerminalJournal* journal) {
    if (!journal) return;
    journal->row_count = 0;
    journal->durable_count = 0;
    journal->live_count = 0;
    journal->scrollback_rows_seen = 0;
    journal->pending_count = 0;
}

void terminal_journal_configure(TerminalJournal* journal, int cap_rows, int cols) {
    if (!journal) return;
    if (cap_rows < 0) cap_rows = 0;
    if (cols < 1) cols = 1;
    if (journal->cap_rows == cap_rows && journal->cols == cols && journal->rows) {
        journal_pending_configure(journal, cols);
        return;
    }

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
    int dropped = 0;
    if (copy_count > cap_rows) {
        journal->drop_count += (unsigned long long)(copy_count - cap_rows);
        dropped = copy_count - cap_rows;
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
    if (dropped > 0) {
        if (dropped >= journal->durable_count) {
            int live_drop = dropped - journal->durable_count;
            journal->durable_count = 0;
            journal->live_count -= live_drop;
            if (journal->live_count < 0) journal->live_count = 0;
        } else {
            journal->durable_count -= dropped;
        }
    }
    if (journal->durable_count > copy_count) journal->durable_count = copy_count;
    journal->live_count = copy_count - journal->durable_count;
    if (journal->live_count < 0) journal->live_count = 0;
    journal->cap_rows = cap_rows;
    journal->cols = cols;
    journal_pending_configure(journal, cols);
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
    if (first < 0 || last < first) {
        if (journal->live_count > 0) {
            int live_start = journal->row_count - journal->live_count;
            const TermCell* live_rows = live_start >= 0
                ? journal->rows + (size_t)live_start * (size_t)journal->cols
                : NULL;
            if (rows_look_like_codex_screen(live_rows, journal->live_count, journal->cols)) {
                journal_commit_pending_codex_rows(journal, NULL, 0, grid->cols, true);
                journal_commit_disappeared_codex_rows(journal, NULL, 0, grid->cols);
                journal->row_count = journal->durable_count;
                journal->live_count = 0;
            }
        }
        return;
    }

    const int snap_count = last - first + 1;
    const TermCell* snap = grid->cells + (size_t)(start_row + first) * (size_t)grid->cols;
    journal->capture_count++;

    (void)cursor_row;
    journal_commit_submitted_command_if_needed(journal, snap, snap_count, grid->cols);
    journal_commit_submitted_codex_prompt_if_needed(journal, snap, snap_count, grid->cols);
    journal_commit_pending_codex_rows(journal, snap, snap_count, grid->cols, false);
    journal_commit_disappeared_codex_rows(journal, snap, snap_count, grid->cols);
    journal_track_codex_pending_rows(journal, snap, snap_count, grid->cols);
    journal->row_count = journal->durable_count;
    journal->live_count = 0;
    journal_sync_scrollback_rows(journal, grid);
    journal_replace_live_tail(journal, snap, snap_count, grid->cols);
}
