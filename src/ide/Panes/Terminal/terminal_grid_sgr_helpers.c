#include "terminal_grid_sgr_helpers.h"

#include <ctype.h>

uint32_t term_grid_pack_rgba(unsigned int r, unsigned int g, unsigned int b) {
    return ((r & 0xFFu) << 24) | ((g & 0xFFu) << 16) | ((b & 0xFFu) << 8) | 0xFFu;
}

uint32_t term_grid_ansi16_color(unsigned int index) {
    static const uint32_t base[16] = {
        0x000000FFu, // black
        0xAA0000FFu, // red
        0x00AA00FFu, // green
        0xAA5500FFu, // yellow
        0x0000AAFFu, // blue
        0xAA00AAFFu, // magenta
        0x00AAAAFFu, // cyan
        0xAAAAAAAAu, // white/gray
        0x555555FFu, // bright black
        0xFF5555FFu, // bright red
        0x55FF55FFu, // bright green
        0xFFFF55FFu, // bright yellow
        0x5555FFFFu, // bright blue
        0xFF55FFFFu, // bright magenta
        0x55FFFFFFu, // bright cyan
        0xFFFFFFFFu, // bright white
    };
    return base[index & 0x0Fu];
}

uint32_t term_grid_ansi256_color(int index) {
    if (index < 0) index = 0;
    if (index > 255) index = 255;

    if (index < 16) {
        return term_grid_ansi16_color((unsigned int)index);
    }

    if (index <= 231) {
        int n = index - 16;
        int r = n / 36;
        int g = (n / 6) % 6;
        int b = n % 6;

        unsigned int rr = (r == 0) ? 0u : (unsigned int)(55 + r * 40);
        unsigned int gg = (g == 0) ? 0u : (unsigned int)(55 + g * 40);
        unsigned int bb = (b == 0) ? 0u : (unsigned int)(55 + b * 40);
        return term_grid_pack_rgba(rr, gg, bb);
    }

    {
        unsigned int v = (unsigned int)(8 + (index - 232) * 10);
        return term_grid_pack_rgba(v, v, v);
    }
}

void term_grid_set_sgr_color(TermGrid* grid, int is_fg, uint32_t color) {
    if (!grid) return;
    if (is_fg) {
        grid->cur_fg = color;
    } else {
        grid->cur_bg = color;
    }
}

void term_grid_reset_style(TermGrid* grid, uint32_t default_fg, uint32_t default_bg) {
    if (!grid) return;
    grid->cur_fg = default_fg;
    grid->cur_bg = default_bg;
    grid->cur_attrs = 0;
}

int term_grid_parse_int(const char* s, int len) {
    int v = 0;
    if (!s || len <= 0) return 0;
    for (int i = 0; i < len; ++i) {
        if (!isdigit((unsigned char)s[i])) return v;
        v = v * 10 + (s[i] - '0');
    }
    return v;
}
