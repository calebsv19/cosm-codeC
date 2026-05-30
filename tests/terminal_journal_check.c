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

static void test_codex_pending_rows_survive_blank_intermediate_capture(void) {
    TermGrid grid;
    term_grid_init(&grid, 8, 80);
    TerminalJournal journal;
    terminal_journal_init(&journal, 64, grid.cols);

    write_row(&grid, 0, "OpenAI Codex (v0.130.0)");
    write_row(&grid, 1, "• Edited");
    write_row(&grid, 2, "  Updated terminal_journal.c with pending rows");
    write_row(&grid, 6, "gpt-5.4-mini medium        .     ~/Desktop/CodeWork/ide");
    write_row(&grid, 7, "\xE2\x80\xBA ");
    terminal_journal_capture_viewport(&journal, &grid, 0, 8, 7);
    assert(journal.pending_count == 2);

    clear_rows(&grid);
    terminal_journal_capture_viewport(&journal, &grid, 0, 8, 0);

    assert(journal.durable_count == 2);
    assert(journal.pending_count == 0);
    assert_row_text(&journal, 0, "• Edited");
    assert_row_text(&journal, 1, "  Updated terminal_journal.c with pending rows");

    write_row(&grid, 0, "OpenAI Codex (v0.130.0)");
    write_row(&grid, 1, "Working (1s - esc to interrupt)");
    write_row(&grid, 6, "gpt-5.4-mini medium        .     ~/Desktop/CodeWork/ide");
    write_row(&grid, 7, "\xE2\x80\xBA ");
    terminal_journal_capture_viewport(&journal, &grid, 0, 8, 7);

    assert(journal.durable_count == 2);
    assert(count_rows_containing(&journal, "OpenAI Codex") == 1);
    assert(count_rows_containing(&journal, "Working (1s") == 1);

    terminal_journal_free(&journal);
    term_grid_free(&grid);
}

static void test_codex_pending_rows_are_not_duplicated_while_visible(void) {
    TermGrid grid;
    term_grid_init(&grid, 8, 80);
    TerminalJournal journal;
    terminal_journal_init(&journal, 64, grid.cols);

    write_row(&grid, 0, "OpenAI Codex (v0.130.0)");
    write_row(&grid, 1, "• Analyzed");
    write_row(&grid, 2, "  Found terminal history gap");
    write_row(&grid, 6, "gpt-5.4-mini medium        .     ~/Desktop/CodeWork/ide");
    write_row(&grid, 7, "\xE2\x80\xBA ");
    terminal_journal_capture_viewport(&journal, &grid, 0, 8, 7);
    terminal_journal_capture_viewport(&journal, &grid, 0, 8, 7);

    assert(journal.durable_count == 0);
    assert(journal.pending_count == 2);
    assert(count_rows_containing(&journal, "• Analyzed") == 1);

    clear_rows(&grid);
    write_row(&grid, 0, "OpenAI Codex (v0.130.0)");
    write_row(&grid, 1, "Thinking (2s - esc to interrupt)");
    write_row(&grid, 6, "gpt-5.4-mini medium        .     ~/Desktop/CodeWork/ide");
    write_row(&grid, 7, "\xE2\x80\xBA ");
    terminal_journal_capture_viewport(&journal, &grid, 0, 8, 7);

    assert(journal.durable_count == 2);
    assert(count_rows_containing(&journal, "• Analyzed") == 1);
    assert(count_rows_containing(&journal, "Found terminal history gap") == 1);

    terminal_journal_free(&journal);
    term_grid_free(&grid);
}

static void test_codex_modal_rows_are_live_only(void) {
    TermGrid grid;
    term_grid_init(&grid, 12, 96);
    TerminalJournal journal;
    terminal_journal_init(&journal, 96, grid.cols);

    write_row(&grid, 0, "OpenAI Codex (v0.130.0)");
    write_row(&grid, 1, "• Story line before modal");
    write_row(&grid, 2, "  Durable assistant text");
    write_row(&grid, 10, "gpt-5.4-mini medium        .     ~/Desktop/CodeWork/ide");
    write_row(&grid, 11, "\xE2\x80\xBA Run /review on my current changes");
    terminal_journal_capture_viewport(&journal, &grid, 0, 12, 11);

    clear_rows(&grid);
    write_row(&grid, 0, "OpenAI Codex (v0.130.0)");
    write_row(&grid, 1, "Select Model and Effort");
    write_row(&grid, 2, "Access legacy models by running codex -m <model_name> or in your config.toml");
    write_row(&grid, 3, "1. gpt-5.5 (default)    Frontier model for complex coding, research, and real-world work.");
    write_row(&grid, 4, "2. gpt-5.4 (current)    Strong model for everyday coding.");
    write_row(&grid, 5, "5. gpt-5.3-codex-spark  Ultra-fast coding model.");
    write_row(&grid, 6, "Press enter to confirm or esc to go back");
    terminal_journal_capture_viewport(&journal, &grid, 0, 12, 6);

    assert(count_rows_containing(&journal, "Select Model and Effort") == 1);
    assert(count_rows_containing(&journal, "gpt-5.3-codex-spark") == 1);

    clear_rows(&grid);
    write_row(&grid, 0, "OpenAI Codex (v0.130.0)");
    write_row(&grid, 1, "• Model changed to gpt-5.3-codex medium");
    write_row(&grid, 10, "gpt-5.3-codex medium      .     ~/Desktop/CodeWork/ide");
    write_row(&grid, 11, "\xE2\x80\xBA ");
    terminal_journal_capture_viewport(&journal, &grid, 0, 12, 11);

    assert(count_rows_containing(&journal, "Select Model and Effort") == 0);
    assert(count_rows_containing(&journal, "gpt-5.3-codex-spark") == 0);
    assert(count_rows_containing(&journal, "Story line before modal") == 1);
    assert(count_rows_containing(&journal, "Durable assistant text") == 1);

    terminal_journal_free(&journal);
    term_grid_free(&grid);
}

static void test_codex_submitted_prompt_becomes_durable(void) {
    TermGrid grid;
    term_grid_init(&grid, 8, 96);
    TerminalJournal journal;
    terminal_journal_init(&journal, 96, grid.cols);

    write_row(&grid, 0, "OpenAI Codex (v0.130.0)");
    write_row(&grid, 6, "gpt-5.4-mini medium        .     ~/Desktop/CodeWork/ide");
    write_row(&grid, 7, "\xE2\x80\xBA Run /review on my current changes");
    terminal_journal_capture_viewport(&journal, &grid, 0, 8, 7);
    assert(journal.durable_count == 0);
    assert(count_rows_containing(&journal, "Run /review on my current changes") == 1);

    clear_rows(&grid);
    write_row(&grid, 0, "OpenAI Codex (v0.130.0)");
    write_row(&grid, 1, "Working (1s - esc to interrupt)");
    write_row(&grid, 6, "gpt-5.4-mini medium        .     ~/Desktop/CodeWork/ide");
    write_row(&grid, 7, "\xE2\x80\xBA ");
    terminal_journal_capture_viewport(&journal, &grid, 0, 8, 7);

    assert(journal.durable_count == 1);
    assert_row_text(&journal, 0, "\xE2\x80\xBA Run /review on my current changes");
    assert(count_rows_containing(&journal, "Run /review on my current changes") == 1);
    assert(count_rows_containing(&journal, "Working (1s") == 1);

    terminal_journal_free(&journal);
    term_grid_free(&grid);
}

static void test_codex_slash_help_and_reasoning_rows_are_live_only(void) {
    TermGrid grid;
    term_grid_init(&grid, 16, 104);
    TerminalJournal journal;
    terminal_journal_init(&journal, 128, grid.cols);

    write_row(&grid, 0, "OpenAI Codex (v0.130.0)");
    write_row(&grid, 1, "• Stable assistant row");
    write_row(&grid, 2, "  Real answer text");
    write_row(&grid, 14, "gpt-5.4-mini medium        .     ~/Desktop/CodeWork/ide");
    write_row(&grid, 15, "\xE2\x80\xBA ");
    terminal_journal_capture_viewport(&journal, &grid, 0, 16, 15);

    clear_rows(&grid);
    write_row(&grid, 0, "\xE2\x95\xAD────────────────");
    write_row(&grid, 1, "  Tip: New Use /fast to enable our fastest inference with increased plan usage.");
    write_row(&grid, 2, "  /model         choose what model and reasoning effort to use");
    write_row(&grid, 3, "  /ide           include current selection, open files, and other context from your IDE");
    write_row(&grid, 4, "  /permissions   choose what Codex is allowed to do");
    write_row(&grid, 5, "  /keymap        remap TUI shortcuts");
    write_row(&grid, 6, "  /vim           toggle Vim mode for the composer");
    write_row(&grid, 7, "  /experimental  toggle experimental features");
    write_row(&grid, 8, "  /approve       approve one retry of a recent auto-review denial");
    write_row(&grid, 9, "  /memories      configure memory use and generation");
    write_row(&grid, 10, "  /mention       mention a file");
    write_row(&grid, 11, "  /mcp           list configured MCP tools; use /mcp verbose for details");
    terminal_journal_capture_viewport(&journal, &grid, 0, 16, 11);

    assert(count_rows_containing(&journal, "/permissions") == 1);
    assert(count_rows_containing(&journal, "Use /fast") == 1);

    clear_rows(&grid);
    write_row(&grid, 0, "OpenAI Codex (v0.130.0)");
    write_row(&grid, 1, "model:     loading   /model to change");
    write_row(&grid, 2, "Select Reasoning Level for gpt-5.3-codex");
    write_row(&grid, 3, "1. Low                         Fast responses with lighter reasoning");
    write_row(&grid, 4, "3. High                        Greater reasoning depth for complex problems");
    write_row(&grid, 5, "MCP client for `computer-use` failed to start: MCP startup failed");
    write_row(&grid, 6, "  closed: initialize response");
    terminal_journal_capture_viewport(&journal, &grid, 0, 16, 6);

    assert(count_rows_containing(&journal, "Reasoning Level") == 1);
    assert(count_rows_containing(&journal, "closed: initialize response") == 1);

    clear_rows(&grid);
    write_row(&grid, 0, "OpenAI Codex (v0.130.0)");
    write_row(&grid, 1, "Working (1s - esc to interrupt)");
    write_row(&grid, 14, "gpt-5.4-mini medium        .     ~/Desktop/CodeWork/ide");
    write_row(&grid, 15, "\xE2\x80\xBA ");
    terminal_journal_capture_viewport(&journal, &grid, 0, 16, 15);

    assert(count_rows_containing(&journal, "/permissions") == 0);
    assert(count_rows_containing(&journal, "Use /fast") == 0);
    assert(count_rows_containing(&journal, "Reasoning Level") == 0);
    assert(count_rows_containing(&journal, "closed: initialize response") == 0);
    assert(count_rows_containing(&journal, "Stable assistant row") == 1);
    assert(count_rows_containing(&journal, "Real answer text") == 1);

    terminal_journal_free(&journal);
    term_grid_free(&grid);
}

static void test_codex_modal_answer_is_not_durable_prompt(void) {
    TermGrid grid;
    term_grid_init(&grid, 12, 104);
    TerminalJournal journal;
    terminal_journal_init(&journal, 128, grid.cols);

    write_row(&grid, 0, "OpenAI Codex (v0.130.0)");
    write_row(&grid, 1, "• Existing assistant text");
    write_row(&grid, 10, "gpt-5.4-mini medium        .     ~/Desktop/CodeWork/ide");
    write_row(&grid, 11, "\xE2\x80\xBA ");
    terminal_journal_capture_viewport(&journal, &grid, 0, 12, 11);

    clear_rows(&grid);
    write_row(&grid, 0, "OpenAI Codex (v0.130.0)");
    write_row(&grid, 1, "Select Model and Effort");
    write_row(&grid, 2, "Keep current model");
    write_row(&grid, 3, "Upgrade to gpt-5.5");
    write_row(&grid, 4, "Press enter to confirm or esc to go back");
    write_row(&grid, 11, "\xE2\x80\xBA Keep current model");
    terminal_journal_capture_viewport(&journal, &grid, 0, 12, 11);

    assert(count_rows_containing(&journal, "Keep current model") == 2);

    clear_rows(&grid);
    write_row(&grid, 0, "OpenAI Codex (v0.130.0)");
    write_row(&grid, 1, "• Model changed to gpt-5.4 medium");
    write_row(&grid, 10, "gpt-5.4 medium             .     ~/Desktop/CodeWork/ide");
    write_row(&grid, 11, "\xE2\x80\xBA ");
    terminal_journal_capture_viewport(&journal, &grid, 0, 12, 11);

    assert(count_rows_containing(&journal, "Keep current model") == 0);
    assert(count_rows_containing(&journal, "Existing assistant text") == 1);
    assert(count_rows_containing(&journal, "Model changed to") == 1);

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
    test_codex_pending_rows_survive_blank_intermediate_capture();
    test_codex_pending_rows_are_not_duplicated_while_visible();
    test_codex_modal_rows_are_live_only();
    test_codex_submitted_prompt_becomes_durable();
    test_codex_slash_help_and_reasoning_rows_are_live_only();
    test_codex_modal_answer_is_not_durable_prompt();
    printf("terminal_journal_check: ok\n");
    return 0;
}
