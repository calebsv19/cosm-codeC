#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "ide/Panes/Terminal/terminal_grid.h"

static TermCell* cell(TermGrid* g, int r, int c) {
    TermCell* out = term_grid_cell(g, r, c);
    assert(out != NULL);
    return out;
}

static void test_chunked_csi_color(void) {
    TermGrid g;
    term_grid_init(&g, 8, 32);

    term_emulator_feed(&g, "\x1b[31", 4);
    term_emulator_feed(&g, "mA", 2);

    assert(cell(&g, 0, 0)->ch == 'A');
    assert(cell(&g, 0, 0)->fg == 0xAA0000FFu);

    term_grid_free(&g);
}

static void test_sgr_256_and_truecolor(void) {
    TermGrid g;
    term_grid_init(&g, 8, 32);

    const char* p1 = "\x1b[38;5;196mX";
    term_emulator_feed(&g, p1, strlen(p1));
    assert(cell(&g, 0, 0)->ch == 'X');
    assert(cell(&g, 0, 0)->fg == 0xFF0000FFu);

    const char* p2 = "\x1b[38;2;1;2;3mY";
    term_emulator_feed(&g, p2, strlen(p2));
    assert(cell(&g, 0, 1)->ch == 'Y');
    assert(cell(&g, 0, 1)->fg == 0x010203FFu);

    term_grid_free(&g);
}

static void test_sgr_bright_and_resets(void) {
    TermGrid g;
    term_grid_init(&g, 8, 32);

    const char* p = "\x1b[91;104mA\x1b[39;49mB";
    term_emulator_feed(&g, p, strlen(p));

    assert(cell(&g, 0, 0)->fg == 0xFF5555FFu);
    assert(cell(&g, 0, 0)->bg == 0x5555FFFFu);
    assert(cell(&g, 0, 1)->fg == 0xFFFFFFFFu);
    assert(cell(&g, 0, 1)->bg == 0x000000FFu);

    term_grid_free(&g);
}

static void test_utf8_decode_and_split_sequence(void) {
    TermGrid g;
    term_grid_init(&g, 8, 32);

    const char* p1 = "A\xe2\x9c";
    const char* p2 = "\x93"; // completes U+2713 CHECK MARK
    term_emulator_feed(&g, p1, strlen(p1));
    term_emulator_feed(&g, p2, strlen(p2));

    assert(cell(&g, 0, 0)->ch == 'A');
    assert(cell(&g, 0, 1)->ch == 0x2713u);

    term_grid_free(&g);
}

static void test_unicode_width_policy(void) {
    TermGrid g;
    term_grid_init(&g, 4, 12);

    const char* combining = "A\xCC\x81" "B"; // U+0301 combining acute does not advance.
    term_emulator_feed(&g, combining, strlen(combining));
    assert(cell(&g, 0, 0)->ch == 'A');
    assert(cell(&g, 0, 1)->ch == 'B');
    assert(g.cursor_row == 0);
    assert(g.cursor_col == 2);

    term_grid_clear(&g);
    const char* wide = "A\xE7\x95\x8C" "B"; // U+754C CJK ideograph has width 2.
    term_emulator_feed(&g, wide, strlen(wide));
    assert(cell(&g, 0, 0)->ch == 'A');
    assert(cell(&g, 0, 1)->ch == 0x754Cu);
    assert(cell(&g, 0, 2)->ch == ' ');
    assert(cell(&g, 0, 3)->ch == 'B');
    assert(g.cursor_row == 0);
    assert(g.cursor_col == 4);

    term_grid_clear(&g);
    const char* emoji = "X\xF0\x9F\x98\x80" "Y"; // U+1F600 reserves two cells.
    term_emulator_feed(&g, emoji, strlen(emoji));
    assert(cell(&g, 0, 0)->ch == 'X');
    assert(cell(&g, 0, 1)->ch == 0x1F600u);
    assert(cell(&g, 0, 2)->ch == ' ');
    assert(cell(&g, 0, 3)->ch == 'Y');
    assert(g.cursor_row == 0);
    assert(g.cursor_col == 4);

    term_grid_free(&g);
}

static void test_osc_swallow(void) {
    TermGrid g;
    term_grid_init(&g, 8, 32);

    const char* p1 = "\x1b]0;ignored-title\x07Z";
    term_emulator_feed(&g, p1, strlen(p1));
    assert(cell(&g, 0, 0)->ch == 'Z');

    term_grid_clear(&g);
    const char* p2 = "\x1b]9;payload\x1b\\Q";
    term_emulator_feed(&g, p2, strlen(p2));
    assert(cell(&g, 0, 0)->ch == 'Q');

    term_grid_free(&g);
}

static void test_csi_char_editing(void) {
    TermGrid g;
    term_grid_init(&g, 4, 12);

    const char* p = "abef\x1b[1;3H\x1b[2@cd";
    term_emulator_feed(&g, p, strlen(p));
    assert(cell(&g, 0, 0)->ch == 'a');
    assert(cell(&g, 0, 1)->ch == 'b');
    assert(cell(&g, 0, 2)->ch == 'c');
    assert(cell(&g, 0, 3)->ch == 'd');
    assert(cell(&g, 0, 4)->ch == 'e');
    assert(cell(&g, 0, 5)->ch == 'f');

    const char* dch = "\x1b[1;3H\x1b[2P";
    term_emulator_feed(&g, dch, strlen(dch));
    assert(cell(&g, 0, 2)->ch == 'e');
    assert(cell(&g, 0, 3)->ch == 'f');

    const char* ech = "\x1b[1;1H\x1b[2X";
    term_emulator_feed(&g, ech, strlen(ech));
    assert(cell(&g, 0, 0)->ch == ' ');
    assert(cell(&g, 0, 1)->ch == ' ');
    assert(cell(&g, 0, 2)->ch == 'e');

    term_grid_free(&g);
}

static void test_csi_line_editing_and_reverse_index(void) {
    TermGrid g;
    term_grid_init(&g, 6, 12);

    const char* lines = "one\ntwo\nthree\nfour";
    term_emulator_feed(&g, lines, strlen(lines));

    const char* insertLine = "\x1b[2;1H\x1b[1L";
    term_emulator_feed(&g, insertLine, strlen(insertLine));
    assert(cell(&g, 1, 0)->ch == ' ');
    assert(cell(&g, 2, 0)->ch == 't');

    const char* deleteLine = "\x1b[2;1H\x1b[1M";
    term_emulator_feed(&g, deleteLine, strlen(deleteLine));
    assert(cell(&g, 1, 0)->ch == 't');
    assert(cell(&g, 2, 0)->ch == 't');

    const char* reverseIndex = "\x1b[1;1H\x1bM";
    term_emulator_feed(&g, reverseIndex, strlen(reverseIndex));
    assert(cell(&g, 0, 0)->ch == ' ');
    assert(cell(&g, 1, 0)->ch == 'o');

    term_grid_free(&g);
}

static void test_terminal_mode_tracking(void) {
    TermGrid g;
    term_grid_init(&g, 6, 12);

    const char* modes = "\x1b[?25l\x1b[?2004h\x1b[?1000h\x1b[2;5r";
    term_emulator_feed(&g, modes, strlen(modes));

    assert(g.cursor_visible == 0);
    assert(g.bracketed_paste == 1);
    assert(g.mouse_mode == 1000);
    assert(g.scroll_top == 1);
    assert(g.scroll_bottom == 4);
    assert(g.cursor_row == 0);
    assert(g.cursor_col == 0);

    const char* resetModes = "\x1b[?25h\x1b[?2004l\x1b[?1000l\x1b[r";
    term_emulator_feed(&g, resetModes, strlen(resetModes));
    assert(g.cursor_visible == 1);
    assert(g.bracketed_paste == 0);
    assert(g.mouse_mode == 0);
    assert(g.scroll_top == 0);
    assert(g.scroll_bottom == g.rows - 1);

    term_grid_free(&g);
}

static void test_plain_overflow_scrollback_retention(void) {
    TermGrid g;
    term_grid_init(&g, 4, 16);
    term_grid_set_scrollback_cap(&g, 8);

    const char* lines =
        "row0\n"
        "row1\n"
        "row2\n"
        "row3\n"
        "row4\n"
        "row5\n";
    term_emulator_feed(&g, lines, strlen(lines));

    assert(term_grid_scrollback_count(&g) == 3);
    assert(term_grid_scrollback_commit_count(&g) == 3);
    assert(term_grid_scrollback_drop_count(&g) == 0);

    const TermCell* first = term_grid_scrollback_row(&g, 0);
    const TermCell* second = term_grid_scrollback_row(&g, 1);
    const TermCell* third = term_grid_scrollback_row(&g, 2);
    assert(first != NULL);
    assert(second != NULL);
    assert(third != NULL);
    assert(first[0].ch == 'r');
    assert(first[3].ch == '0');
    assert(second[3].ch == '1');
    assert(third[3].ch == '2');

    assert(cell(&g, 0, 3)->ch == '3');
    assert(cell(&g, 1, 3)->ch == '4');
    assert(cell(&g, 2, 3)->ch == '5');
    assert(g.cursor_row == 3);
    assert(g.cursor_col == 0);

    term_grid_free(&g);
}

int main(void) {
    test_chunked_csi_color();
    test_sgr_256_and_truecolor();
    test_sgr_bright_and_resets();
    test_utf8_decode_and_split_sequence();
    test_unicode_width_policy();
    test_osc_swallow();
    test_csi_char_editing();
    test_csi_line_editing_and_reverse_index();
    test_terminal_mode_tracking();
    test_plain_overflow_scrollback_retention();

    printf("terminal_grid_phase1_check: ok\n");
    return 0;
}
