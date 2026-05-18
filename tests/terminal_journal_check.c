#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "ide/Panes/Terminal/terminal_grid.h"
#include "ide/Panes/Terminal/terminal_journal.h"

static void write_row(TermGrid* grid, int row, const char* text) {
    assert(grid != NULL);
    assert(row >= 0 && row < grid->rows);
    TermCell* cells = term_grid_cell(grid, row, 0);
    assert(cells != NULL);
    for (int c = 0; c < grid->cols; ++c) {
        cells[c].ch = ' ';
        cells[c].fg = 0xFFFFFFFFu;
        cells[c].bg = 0x000000FFu;
        cells[c].attrs = 0;
    }
    for (int c = 0; text && text[c] && c < grid->cols; ++c) {
        cells[c].ch = (unsigned char)text[c];
    }
}

static void assert_row_text(const TerminalJournal* journal, int row, const char* text) {
    const TermCell* cells = terminal_journal_row(journal, row);
    assert(cells != NULL);
    int len = (int)strlen(text);
    for (int c = 0; c < len; ++c) {
        assert(cells[c].ch == (unsigned char)text[c]);
    }
}

static void test_journal_merges_expanded_viewport_prefix_and_suffix(void) {
    TermGrid grid;
    term_grid_init(&grid, 6, 16);
    TerminalJournal journal;
    terminal_journal_init(&journal, 32, grid.cols);

    write_row(&grid, 0, "row0");
    write_row(&grid, 1, "row1");
    write_row(&grid, 2, "row2");
    write_row(&grid, 3, "row3");
    write_row(&grid, 4, "row4");

    terminal_journal_capture_viewport(&journal, &grid, 1, 3, 3);
    assert(terminal_journal_count(&journal) == 3);
    assert_row_text(&journal, 0, "row1");
    assert_row_text(&journal, 2, "row3");

    terminal_journal_capture_viewport(&journal, &grid, 0, 5, 4);
    assert(terminal_journal_count(&journal) == 5);
    assert_row_text(&journal, 0, "row0");
    assert_row_text(&journal, 1, "row1");
    assert_row_text(&journal, 2, "row2");
    assert_row_text(&journal, 3, "row3");
    assert_row_text(&journal, 4, "row4");

    terminal_journal_free(&journal);
    term_grid_free(&grid);
}

static void test_journal_appends_new_tail_from_scrolling_snapshot(void) {
    TermGrid grid;
    term_grid_init(&grid, 6, 16);
    TerminalJournal journal;
    terminal_journal_init(&journal, 32, grid.cols);

    write_row(&grid, 0, "alpha");
    write_row(&grid, 1, "bravo");
    write_row(&grid, 2, "charlie");
    write_row(&grid, 3, "delta");

    terminal_journal_capture_viewport(&journal, &grid, 0, 3, 2);
    terminal_journal_capture_viewport(&journal, &grid, 1, 3, 3);

    assert(terminal_journal_count(&journal) == 4);
    assert_row_text(&journal, 0, "alpha");
    assert_row_text(&journal, 1, "bravo");
    assert_row_text(&journal, 2, "charlie");
    assert_row_text(&journal, 3, "delta");

    terminal_journal_free(&journal);
    term_grid_free(&grid);
}

static void test_journal_preserves_internal_blank_rows(void) {
    TermGrid grid;
    term_grid_init(&grid, 5, 16);
    TerminalJournal journal;
    terminal_journal_init(&journal, 32, grid.cols);

    write_row(&grid, 0, "top");
    write_row(&grid, 2, "bottom");

    terminal_journal_capture_viewport(&journal, &grid, 0, 3, 2);

    assert(terminal_journal_count(&journal) == 3);
    assert_row_text(&journal, 0, "top");
    assert(terminal_journal_row(&journal, 1)[0].ch == ' ');
    assert_row_text(&journal, 2, "bottom");

    terminal_journal_free(&journal);
    term_grid_free(&grid);
}

static void test_journal_replaces_live_prompt_row_during_typing(void) {
    TermGrid grid;
    term_grid_init(&grid, 4, 32);
    TerminalJournal journal;
    terminal_journal_init(&journal, 32, grid.cols);

    write_row(&grid, 0, "[Terminal] Started shell");
    write_row(&grid, 1, "calebsv@Mac sim %");
    terminal_journal_capture_viewport(&journal, &grid, 0, 2, 1);
    assert(terminal_journal_count(&journal) == 2);

    write_row(&grid, 1, "calebsv@Mac sim % c");
    terminal_journal_capture_viewport(&journal, &grid, 0, 2, 1);
    assert(terminal_journal_count(&journal) == 2);
    assert_row_text(&journal, 1, "calebsv@Mac sim % c");

    write_row(&grid, 1, "calebsv@Mac sim % codex");
    terminal_journal_capture_viewport(&journal, &grid, 0, 2, 1);
    assert(terminal_journal_count(&journal) == 2);
    assert_row_text(&journal, 1, "calebsv@Mac sim % codex");

    write_row(&grid, 2, "Codex output begins");
    terminal_journal_capture_viewport(&journal, &grid, 0, 3, 2);
    assert(terminal_journal_count(&journal) == 3);
    assert_row_text(&journal, 1, "calebsv@Mac sim % codex");
    assert_row_text(&journal, 2, "Codex output begins");

    terminal_journal_free(&journal);
    term_grid_free(&grid);
}

int main(void) {
    test_journal_merges_expanded_viewport_prefix_and_suffix();
    test_journal_appends_new_tail_from_scrolling_snapshot();
    test_journal_preserves_internal_blank_rows();
    test_journal_replaces_live_prompt_row_during_typing();
    printf("terminal_journal_check: ok\n");
    return 0;
}
