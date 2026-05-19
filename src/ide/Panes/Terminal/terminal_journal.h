#ifndef TERMINAL_JOURNAL_H
#define TERMINAL_JOURNAL_H

#include "ide/Panes/Terminal/terminal_grid.h"

typedef struct TerminalJournal {
    TermCell* rows;
    int row_count;
    int durable_count;
    int live_count;
    int cap_rows;
    int cols;
    int scrollback_rows_seen;
    unsigned long long capture_count;
    unsigned long long insert_count;
    unsigned long long append_count;
    unsigned long long drop_count;
} TerminalJournal;

void terminal_journal_init(TerminalJournal* journal, int cap_rows, int cols);
void terminal_journal_free(TerminalJournal* journal);
void terminal_journal_clear(TerminalJournal* journal);
void terminal_journal_configure(TerminalJournal* journal, int cap_rows, int cols);
int terminal_journal_count(const TerminalJournal* journal);
const TermCell* terminal_journal_row(const TerminalJournal* journal, int index);
void terminal_journal_capture_viewport(TerminalJournal* journal,
                                       const TermGrid* grid,
                                       int start_row,
                                       int row_count,
                                       int cursor_row);
int terminal_journal_find_last_equal_row(const TerminalJournal* journal,
                                         const TermCell* row,
                                         int cols);

#endif
