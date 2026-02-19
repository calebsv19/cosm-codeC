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

int main(void) {
    test_chunked_csi_color();
    test_sgr_256_and_truecolor();
    test_sgr_bright_and_resets();
    test_utf8_decode_and_split_sequence();
    test_osc_swallow();

    printf("terminal_grid_phase1_check: ok\n");
    return 0;
}
