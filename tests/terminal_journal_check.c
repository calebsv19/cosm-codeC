#include <assert.h>
#include <stdbool.h>
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

static void clear_rows(TermGrid* grid) {
    assert(grid != NULL);
    for (int r = 0; r < grid->rows; ++r) {
        write_row(grid, r, "");
    }
    grid->used_rows = grid->rows;
}

static void assert_row_text(const TerminalJournal* journal, int row, const char* text) {
    const TermCell* cells = terminal_journal_row(journal, row);
    assert(cells != NULL);
    int len = (int)strlen(text);
    for (int c = 0; c < len; ++c) {
        assert(cells[c].ch == (unsigned char)text[c]);
    }
}

static int count_rows_containing(const TerminalJournal* journal, const char* text) {
    assert(journal != NULL);
    assert(text != NULL);
    int count = 0;
    int len = (int)strlen(text);
    for (int r = 0; r < terminal_journal_count(journal); ++r) {
        const TermCell* cells = terminal_journal_row(journal, r);
        assert(cells != NULL);
        for (int c = 0; c <= journal->cols - len; ++c) {
            bool match = true;
            for (int i = 0; i < len; ++i) {
                if (cells[c + i].ch != (unsigned char)text[i]) {
                    match = false;
                    break;
                }
            }
            if (match) {
                count++;
                break;
            }
        }
    }
    return count;
}

static void test_journal_replaces_live_viewport_on_expand(void) {
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

static void test_journal_replaces_shifted_live_viewport_without_append(void) {
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

    assert(terminal_journal_count(&journal) == 3);
    assert_row_text(&journal, 0, "bravo");
    assert_row_text(&journal, 1, "charlie");
    assert_row_text(&journal, 2, "delta");

    terminal_journal_free(&journal);
    term_grid_free(&grid);
}

static void test_journal_appends_explicit_scrollback_rows_as_durable(void) {
    TermGrid grid;
    term_grid_init(&grid, 3, 16);
    term_grid_set_scrollback_cap(&grid, 16);
    TerminalJournal journal;
    terminal_journal_init(&journal, 32, grid.cols);

    term_emulator_feed(&grid, "alpha\nbravo\ncharlie\ndelta\n", 26);
    terminal_journal_capture_viewport(&journal, &grid, 0, grid.rows, grid.cursor_row);

    assert(term_grid_scrollback_count(&grid) > 0);
    assert(journal.durable_count == term_grid_scrollback_count(&grid));
    assert_row_text(&journal, 0, "alpha");

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

static void test_journal_replaces_repainted_codex_footer(void) {
    TermGrid grid;
    term_grid_init(&grid, 6, 80);
    TerminalJournal journal;
    terminal_journal_init(&journal, 32, grid.cols);

    write_row(&grid, 0, "OpenAI Codex");
    write_row(&grid, 1, "Run /review on my current changes");
    write_row(&grid, 2, "gpt-5.4-mini medium        .     ~/Desktop/CodeWork/gravity_orbit_sim");
    terminal_journal_capture_viewport(&journal, &grid, 0, 3, 1);
    assert(terminal_journal_count(&journal) == 3);

    write_row(&grid, 2, "gpt-5.4-mini high          .     ~/Desktop/CodeWork/gravity_orbit_sim");
    terminal_journal_capture_viewport(&journal, &grid, 0, 3, 1);
    assert(terminal_journal_count(&journal) == 3);
    assert_row_text(&journal, 2, "gpt-5.4-mini high");

    terminal_journal_free(&journal);
    term_grid_free(&grid);
}

static void test_journal_replaces_repainted_progress_status(void) {
    TermGrid grid;
    term_grid_init(&grid, 6, 80);
    TerminalJournal journal;
    terminal_journal_init(&journal, 32, grid.cols);

    write_row(&grid, 0, "OpenAI Codex");
    write_row(&grid, 1, "Booting MCP server: codex_apps (0s - esc to interrupt)");
    terminal_journal_capture_viewport(&journal, &grid, 0, 2, 1);
    assert(terminal_journal_count(&journal) == 2);

    write_row(&grid, 1, "Booting MCP server: codex_apps (1s - esc to interrupt)");
    terminal_journal_capture_viewport(&journal, &grid, 0, 2, 1);
    assert(terminal_journal_count(&journal) == 2);
    assert_row_text(&journal, 1, "Booting MCP server: codex_apps (1s");

    terminal_journal_free(&journal);
    term_grid_free(&grid);
}

static void test_codex_startup_repaint_and_resize_stays_live(void) {
    TermGrid grid;
    term_grid_init(&grid, 8, 80);
    TerminalJournal journal;
    terminal_journal_init(&journal, 64, grid.cols);

    write_row(&grid, 0, "[Terminal] Started shell");
    write_row(&grid, 1, "calebsv@Mac ide % codex");
    terminal_journal_capture_viewport(&journal, &grid, 0, 2, 1);
    assert(terminal_journal_count(&journal) == 2);

    clear_rows(&grid);
    write_row(&grid, 0, "OpenAI Codex (v0.130.0)");
    write_row(&grid, 1, "Booting MCP server: codex_apps (0s - esc to interrupt)");
    write_row(&grid, 6, "gpt-5.4-mini medium        .     ~/Desktop/CodeWork/ide");
    write_row(&grid, 7, "\xE2\x80\xBA ");
    terminal_journal_capture_viewport(&journal, &grid, 0, 8, 7);
    assert_row_text(&journal, 0, "calebsv@Mac ide % codex");
    assert(count_rows_containing(&journal, "OpenAI Codex") == 1);

    write_row(&grid, 1, "Booting MCP server: codex_apps (1s - esc to interrupt)");
    terminal_journal_capture_viewport(&journal, &grid, 0, 8, 7);
    assert_row_text(&journal, 0, "calebsv@Mac ide % codex");
    assert(count_rows_containing(&journal, "OpenAI Codex") == 1);
    assert(count_rows_containing(&journal, "Booting MCP server") == 1);

    term_grid_resize(&grid, 10, 80);
    clear_rows(&grid);
    write_row(&grid, 0, "OpenAI Codex (v0.130.0)");
    write_row(&grid, 1, "Booting MCP server: codex_apps (2s - esc to interrupt)");
    write_row(&grid, 8, "gpt-5.4-mini medium        .     ~/Desktop/CodeWork/ide");
    write_row(&grid, 9, "\xE2\x80\xBA ");
    terminal_journal_configure(&journal, 64, grid.cols);
    terminal_journal_capture_viewport(&journal, &grid, 0, 10, 9);
    assert_row_text(&journal, 0, "calebsv@Mac ide % codex");
    assert(count_rows_containing(&journal, "OpenAI Codex") == 1);
    assert(count_rows_containing(&journal, "Booting MCP server") == 1);

    terminal_journal_free(&journal);
    term_grid_free(&grid);
}

static void test_codex_disappeared_transcript_rows_become_scrollback(void) {
    TermGrid grid;
    term_grid_init(&grid, 8, 80);
    TerminalJournal journal;
    terminal_journal_init(&journal, 64, grid.cols);

    write_row(&grid, 0, "OpenAI Codex (v0.130.0)");
    write_row(&grid, 1, "• Explored");
    write_row(&grid, 2, "  Read architecture.md and terminal_journal.c");
    write_row(&grid, 6, "gpt-5.4-mini medium        .     ~/Desktop/CodeWork/ide");
    write_row(&grid, 7, "\xE2\x80\xBA ");
    terminal_journal_capture_viewport(&journal, &grid, 0, 8, 7);
    assert(terminal_journal_count(&journal) == 8);

    clear_rows(&grid);
    write_row(&grid, 0, "OpenAI Codex (v0.130.0)");
    write_row(&grid, 1, "Working (1s - esc to interrupt)");
    write_row(&grid, 6, "gpt-5.4-mini medium        .     ~/Desktop/CodeWork/ide");
    write_row(&grid, 7, "\xE2\x80\xBA ");
    terminal_journal_capture_viewport(&journal, &grid, 0, 8, 7);

    assert(journal.durable_count == 2);
    assert_row_text(&journal, 0, "• Explored");
    assert_row_text(&journal, 1, "  Read architecture.md and terminal_journal.c");
    assert(count_rows_containing(&journal, "OpenAI Codex") == 1);
    assert(count_rows_containing(&journal, "Working (1s") == 1);

    terminal_journal_free(&journal);
    term_grid_free(&grid);
}

int main(void) {
    test_journal_replaces_live_viewport_on_expand();
    test_journal_replaces_shifted_live_viewport_without_append();
    test_journal_appends_explicit_scrollback_rows_as_durable();
    test_journal_preserves_internal_blank_rows();
    test_journal_replaces_live_prompt_row_during_typing();
    test_journal_replaces_repainted_codex_footer();
    test_journal_replaces_repainted_progress_status();
    test_codex_startup_repaint_and_resize_stays_live();
    test_codex_disappeared_transcript_rows_become_scrollback();
    printf("terminal_journal_check: ok\n");
    return 0;
}
