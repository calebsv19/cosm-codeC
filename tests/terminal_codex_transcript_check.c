#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "ide/Panes/Terminal/terminal_grid.h"

static TermCell* at(TermGrid* g, int r, int c) {
    TermCell* cell = term_grid_cell(g, r, c);
    assert(cell != NULL);
    return cell;
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
    test_wrapping_stability();
    printf("terminal_codex_transcript_check: ok\n");
    return 0;
}
