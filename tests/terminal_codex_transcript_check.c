#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "ide/Panes/Terminal/terminal_grid.h"

static TermCell* at(TermGrid* g, int r, int c) {
    TermCell* cell = term_grid_cell(g, r, c);
    assert(cell != NULL);
    return cell;
}

static void feed(TermGrid* g, const char* text) {
    term_emulator_feed(g, text, strlen(text));
}

static void feed_box_separator(TermGrid* g, int cells) {
    feed(g, "\x1b[90m");
    for (int i = 0; i < cells; ++i) {
        term_emulator_feed(g, "\xE2\x94\x80", 3); // U+2500 box drawings light horizontal.
    }
    feed(g, "\x1b[0m\n");
}

static void test_codex_like_transcript(void) {
    TermGrid g;
    term_grid_init(&g, 10, 40);

    const char* transcript =
        "\x1b[1mPlan:\x1b[0m\n"
        "1. Parse \x1b[38;5;45mANSI\x1b[39m output\n"
        "2. Render UTF-8: \xe2\x9c\x93 \xe2\x94\x82 \xe2\x94\x80\n"
        "\x1b]8;;https://example.com\x07link\x1b]8;;\x07 done\n"
        "status: \x1b[32mok\x1b[0m\n";

    term_emulator_feed(&g, transcript, strlen(transcript));

    // "Plan:" starts row 0, bold.
    assert(at(&g, 0, 0)->ch == 'P');
    assert((at(&g, 0, 0)->attrs & (1u << 0)) != 0);

    // ANSI 256-color text applied and reset around "ANSI".
    // Row 1: "1. Parse ANSI output"
    assert(at(&g, 1, 9)->ch == 'A');
    assert(at(&g, 1, 9)->fg == 0x00D7FFFFu);
    assert(at(&g, 1, 13)->ch == ' ');
    assert(at(&g, 1, 13)->fg == 0xFFFFFFFFu);

    // UTF-8 glyphs are preserved as codepoints.
    // Row 2: "... UTF-8: ✓ │ ─"
    assert(at(&g, 2, 17)->ch == 0x2713u);
    assert(at(&g, 2, 19)->ch == 0x2502u);
    assert(at(&g, 2, 21)->ch == 0x2500u);

    // OSC hyperlink metadata is swallowed; only visible link text remains.
    // Row 3 starts with "link done"
    assert(at(&g, 3, 0)->ch == 'l');
    assert(at(&g, 3, 1)->ch == 'i');
    assert(at(&g, 3, 2)->ch == 'n');
    assert(at(&g, 3, 3)->ch == 'k');

    // Final status line color.
    assert(at(&g, 4, 8)->ch == 'o');
    assert(at(&g, 4, 8)->fg == 0x00AA00FFu);

    term_grid_free(&g);
}

static void test_codex_ui_pattern_replay(void) {
    TermGrid g;
    term_grid_init(&g, 12, 80);

    feed(&g, "\x1b[1m\xE2\x80\xA2 Explored\x1b[0m\n");
    feed(&g, "  \x1b[38;5;45mRead\x1b[39m architecture.md, README.md, Makefile, current_truth.md\n");
    feed_box_separator(&g, 64);
    feed(&g, "\xE2\x80\xA2 This repo is a C/SDL simulation app called \x1b[36mgravity_orbit_sim\x1b[39m.\n");
    feed(&g, "- Entry/app wrapper: \x1b[36msrc/main.c\x1b[39m, \x1b[36msrc/app/gravity_orbit_sim_app_main.c\x1b[39m\n");
    feed_box_separator(&g, 64);
    feed(&g, "\x1b[33mgpt-5.3-codex medium\x1b[39m \xC2\xB7 \x1b[32m~/Desktop/CodeWork/gravity_orbit_sim\x1b[39m\n");
    feed(&g, "\xE2\x80\xBA Write tests for @filename");

    assert(at(&g, 0, 0)->ch == 0x2022u);
    assert((at(&g, 0, 2)->attrs & (1u << 0)) != 0);

    assert(at(&g, 1, 2)->ch == 'R');
    assert(at(&g, 1, 2)->fg == 0x00D7FFFFu);
    assert(at(&g, 1, 7)->ch == 'a');
    assert(at(&g, 1, 7)->fg == 0xFFFFFFFFu);

    for (int c = 0; c < 64; ++c) {
        assert(at(&g, 2, c)->ch == 0x2500u);
        assert(at(&g, 2, c)->fg == 0x555555FFu);
        assert(at(&g, 5, c)->ch == 0x2500u);
        assert(at(&g, 5, c)->fg == 0x555555FFu);
    }

    assert(at(&g, 3, 45)->ch == 'g');
    assert(at(&g, 3, 45)->fg == 0x00AAAAFFu);
    assert(at(&g, 4, 21)->ch == 's');
    assert(at(&g, 4, 21)->fg == 0x00AAAAFFu);

    assert(at(&g, 6, 0)->ch == 'g');
    assert(at(&g, 6, 0)->fg == 0xAA5500FFu);
    assert(at(&g, 6, 21)->ch == 0x00B7u);
    assert(at(&g, 6, 23)->ch == '~');
    assert(at(&g, 6, 23)->fg == 0x00AA00FFu);

    assert(at(&g, 7, 0)->ch == 0x203Au);
    assert(at(&g, 7, 2)->ch == 'W');
    assert(g.cursor_row == 7);
    assert(g.cursor_col == 27);
    assert(g.cursor_visible == 1);

    term_grid_free(&g);
}

static void test_wrapping_stability(void) {
    TermGrid g;
    term_grid_init(&g, 4, 8);

    const char* line = "abcdefghijk";
    term_emulator_feed(&g, line, strlen(line));

    // Expect wrap at column 8.
    assert(at(&g, 0, 0)->ch == 'a');
    assert(at(&g, 0, 7)->ch == 'h');
    assert(at(&g, 1, 0)->ch == 'i');
    assert(at(&g, 1, 2)->ch == 'k');

    term_grid_free(&g);
}

int main(void) {
    test_codex_like_transcript();
    test_codex_ui_pattern_replay();
    test_wrapping_stability();
    printf("terminal_codex_transcript_check: ok\n");
    return 0;
}
