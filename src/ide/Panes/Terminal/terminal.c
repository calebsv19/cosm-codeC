#include "terminal.h"
#include "core/Clipboard/clipboard.h"
#include "ide/UI/scroll_manager.h"
#include "core/Terminal/terminal_backend.h"
#include "ide/Panes/Terminal/terminal_grid.h"
#include "engine/Render/render_font.h"
#include "engine/Render/render_text_helpers.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/select.h>
#include <SDL2/SDL_ttf.h>

typedef struct {
    bool selecting;
    bool hasSelection;
    int anchorLine;
    int anchorCol;
    int cursorLine;
    int cursorCol;
} TerminalSelectionState;

static TerminalSelectionState g_selection = {0};
static PaneScrollState g_terminalScrollState;
static bool g_terminalScrollInitialized = false;
static SDL_Rect g_terminalScrollTrack = {0};
static SDL_Rect g_terminalScrollThumb = {0};
static bool g_terminalFollowOutput = true;
static TerminalBackend* g_terminalBackend = NULL;
static size_t g_backendConsumed = 0;
static bool g_backendExitNotified = false;
TermGrid g_termGrid;
int g_gridRows = 512;   // generous scrollback
int g_gridCols = 120;
int g_cellWidth = 8;
int g_cellHeight = TERMINAL_LINE_HEIGHT;
static int g_lastViewportW = -1;
static int g_lastViewportH = -1;
static const PaneScrollConfig kTerminalScrollConfig = {
    .line_height_px = TERMINAL_LINE_HEIGHT,
    .deceleration_px = 0.0f,
    .allow_negative = false,
};

static void ensure_terminal_scroll_state(void) {
    if (!g_terminalScrollInitialized) {
        scroll_state_init(&g_terminalScrollState, &kTerminalScrollConfig);
        g_terminalScrollInitialized = true;
    }
}

static void terminal_jump_to_bottom(void) {
    ensure_terminal_scroll_state();
    PaneScrollState* scroll = &g_terminalScrollState;
    int lastRowUsed = terminal_last_used_row();
    int contentRows = (lastRowUsed >= 0) ? (lastRowUsed + 1) : 1;
    float contentHeight = (float)g_cellHeight * (float)contentRows;
    scroll_state_set_content_height(scroll, contentHeight);
    float maxOffset = contentHeight - scroll->viewport_height_px;
    if (maxOffset < 0.0f) maxOffset = 0.0f;
    scroll->target_offset_px = maxOffset;
    scroll->offset_px = maxOffset;
}

int terminal_line_length(int row, bool trim_trailing) {
    if (!g_termGrid.cells || row < 0 || row >= g_termGrid.rows) return 0;
    int lastNonSpace = -1;
    for (int c = 0; c < g_termGrid.cols; ++c) {
        TermCell* cell = term_grid_cell(&g_termGrid, row, c);
        char ch = cell ? (char)(cell->ch ? cell->ch : ' ') : ' ';
        if (ch != ' ' && ch != '\0') {
            lastNonSpace = c;
        }
    }
    if (!trim_trailing) return g_termGrid.cols;
    return (lastNonSpace >= 0) ? lastNonSpace + 1 : 0;
}

int terminal_line_to_string(int row, char* out, int cap, bool trim_trailing) {
    if (!out || cap <= 0) return 0;
    if (!g_termGrid.cells || row < 0 || row >= g_termGrid.rows) {
        out[0] = '\0';
        return 0;
    }
    int len = terminal_line_length(row, trim_trailing);
    if (len > cap - 1) len = cap - 1;
    for (int c = 0; c < len; ++c) {
        TermCell* cell = term_grid_cell(&g_termGrid, row, c);
        char ch = cell ? (char)(cell->ch ? cell->ch : ' ') : ' ';
        out[c] = (ch >= 0x20 && ch < 0x7F) ? ch : ' ';
    }
    out[len] = '\0';
    return len;
}

int terminal_last_used_row(void) {
    if (!g_termGrid.cells || g_termGrid.rows <= 0) return -1;
    for (int r = g_termGrid.rows - 1; r >= 0; --r) {
        for (int c = 0; c < g_termGrid.cols; ++c) {
            TermCell* cell = term_grid_cell(&g_termGrid, r, c);
            char ch = cell ? (char)(cell->ch ? cell->ch : ' ') : ' ';
            if (ch != ' ') {
                return r;
            }
        }
    }
    return -1;
}

static void clamp_selection_position(int* line, int* column) {
    if (*line < 0) *line = 0;
    if (*line >= g_termGrid.rows) *line = g_termGrid.rows - 1;
    if (*line < 0) {
        *line = 0;
        *column = 0;
        return;
    }

    int len = terminal_line_length(*line, true);
    if (*column < 0) *column = 0;
    if (*column > len) *column = len;
}

static void normalize_selection(int* startLine, int* startCol, int* endLine, int* endCol) {
    int sLine = g_selection.anchorLine;
    int sCol = g_selection.anchorCol;
    int eLine = g_selection.cursorLine;
    int eCol = g_selection.cursorCol;

    if (sLine > eLine || (sLine == eLine && sCol > eCol)) {
        int tmpLine = sLine;
        int tmpCol = sCol;
        sLine = eLine;
        sCol = eCol;
        eLine = tmpLine;
        eCol = tmpCol;
    }

    *startLine = sLine;
    *startCol = sCol;
    *endLine = eLine;
    *endCol = eCol;
}

void initTerminal() {
    terminal_clear_selection();
    g_backendConsumed = 0;
    g_backendExitNotified = false;
    term_grid_init(&g_termGrid, g_gridRows, g_gridCols);
}

void printToTerminal(const char* text) {
    if (!text) return;
    size_t len = strlen(text);
    term_emulator_feed(&g_termGrid, text, len);
}

void clearTerminal() {
    terminal_clear_selection();
    ensure_terminal_scroll_state();
    g_terminalScrollState.offset_px = 0.0f;
    g_terminalScrollState.target_offset_px = 0.0f;
    term_grid_clear(&g_termGrid);
    g_lastViewportW = -1;
    g_lastViewportH = -1;
}

void terminal_begin_selection(int line, int column) {
    if (!g_termGrid.cells || g_termGrid.rows == 0) {
        terminal_clear_selection();
        return;
    }
    clamp_selection_position(&line, &column);
    g_selection.selecting = true;
    g_selection.hasSelection = false;
    g_selection.anchorLine = line;
    g_selection.anchorCol = column;
    g_selection.cursorLine = line;
    g_selection.cursorCol = column;
}

void terminal_update_selection(int line, int column) {
    if (!g_selection.selecting || g_termGrid.rows == 0) return;
    clamp_selection_position(&line, &column);
    g_selection.cursorLine = line;
    g_selection.cursorCol = column;
    g_selection.hasSelection =
        (g_selection.anchorLine != g_selection.cursorLine) ||
        (g_selection.anchorCol != g_selection.cursorCol);
}

void terminal_end_selection(void) {
    g_selection.selecting = false;
}

void terminal_clear_selection(void) {
    g_selection = (TerminalSelectionState){0};
}

bool terminal_get_selection_bounds(int* startLine, int* startCol, int* endLine, int* endCol) {
    if (!terminal_has_selection()) return false;
    int sLine, sCol, eLine, eCol;
    normalize_selection(&sLine, &sCol, &eLine, &eCol);
    clamp_selection_position(&sLine, &sCol);
    clamp_selection_position(&eLine, &eCol);
    *startLine = sLine;
    *startCol = sCol;
    *endLine = eLine;
    *endCol = eCol;
    return true;
}

bool terminal_has_selection(void) {
    if (!g_selection.hasSelection) return false;
    if (!g_termGrid.cells || g_termGrid.rows == 0) return false;
    if (g_selection.anchorLine == g_selection.cursorLine &&
        g_selection.anchorCol == g_selection.cursorCol) {
        return false;
    }
    return true;
}

bool terminal_copy_selection_to_clipboard(void) {
    if (!terminal_has_selection()) return false;

    int startLine, startCol, endLine, endCol;
    normalize_selection(&startLine, &startCol, &endLine, &endCol);
    clamp_selection_position(&startLine, &startCol);
    clamp_selection_position(&endLine, &endCol);

    size_t total = 0;
    for (int line = startLine; line <= endLine; ++line) {
        int lineLen = terminal_line_length(line, true);
        int from = (line == startLine) ? startCol : 0;
        int to = (line == endLine) ? endCol : lineLen;
        if (from < 0) from = 0;
        if (to < from) to = from;
        if (to > lineLen) to = lineLen;
        total += (size_t)(to - from);
        if (line != endLine) total++;
    }

    char* buffer = (char*)malloc(total + 1);
    if (!buffer) return false;

    size_t offset = 0;
    for (int line = startLine; line <= endLine; ++line) {
        int lineLen = terminal_line_length(line, true);
        int from = (line == startLine) ? startCol : 0;
        int to = (line == endLine) ? endCol : lineLen;
        if (from < 0) from = 0;
        if (to < from) to = from;
        if (to > lineLen) to = lineLen;
        int chunk = to - from;
        if (chunk > 0) {
            char* temp = (char*)malloc((size_t)lineLen + 1);
            if (!temp) { free(buffer); return false; }
            terminal_line_to_string(line, temp, lineLen + 1, true);
            memcpy(buffer + offset, temp + from, (size_t)chunk);
            offset += (size_t)chunk;
            free(temp);
        }
        if (line != endLine) {
            buffer[offset++] = '\n';
        }
    }
    buffer[offset] = '\0';

    bool success = clipboard_copy_text(buffer);
    free(buffer);
    return success;
}

PaneScrollState* terminal_get_scroll_state(void) {
    ensure_terminal_scroll_state();
    return &g_terminalScrollState;
}

void terminal_set_scroll_track(const SDL_Rect* track, const SDL_Rect* thumb) {
    if (track) {
        g_terminalScrollTrack = *track;
    } else {
        g_terminalScrollTrack = (SDL_Rect){0};
    }
    if (thumb) {
        g_terminalScrollThumb = *thumb;
    } else {
        g_terminalScrollThumb = (SDL_Rect){0};
    }
}

void terminal_get_scroll_track(SDL_Rect* track, SDL_Rect* thumb) {
    if (track) *track = g_terminalScrollTrack;
    if (thumb) *thumb = g_terminalScrollThumb;
}

void terminal_set_follow_output(bool follow) {
    ensure_terminal_scroll_state();
    g_terminalFollowOutput = follow;
}

bool terminal_is_following_output(void) {
    ensure_terminal_scroll_state();
    return g_terminalFollowOutput;
}

static void terminal_flush_backend_output(void) {
    if (!g_terminalBackend) return;

    size_t len = 0;
    const char* data = terminal_backend_buffer(g_terminalBackend, &len);
    if (data && len > g_backendConsumed) {
        size_t chunkLen = len - g_backendConsumed;
        char* raw = (char*)malloc(chunkLen + 1);
        if (raw) {
            memcpy(raw, data + g_backendConsumed, chunkLen);
            raw[chunkLen] = '\0';
            // Debug: log incoming chunk and cursor state
            printf("[TerminalDebug] Chunk (%zu bytes): \"", chunkLen);
            for (size_t i = 0; i < chunkLen; ++i) {
                unsigned char c = (unsigned char)raw[i];
                if (c == '\n') printf("\\n");
                else if (c == '\r') printf("\\r");
                else if (c == '\t') printf("\\t");
                else if (c < 32 || c > 126) printf("\\x%02X", c);
                else putchar(c);
            }
            printf("\"\n");
            // Feed raw bytes to emulator
            term_emulator_feed(&g_termGrid, raw, chunkLen);
            free(raw);
        }
        g_backendConsumed = len;
    }

    if (g_terminalBackend->dead && !g_backendExitNotified) {
        printToTerminal("[Terminal] Shell process exited.\n");
        g_backendExitNotified = true;
    }
}

void terminal_send_text(const char* text, size_t len) {
    if (!text || len == 0) return;
    if (!g_terminalBackend || g_terminalBackend->dead) return;
    terminal_backend_send_input(g_terminalBackend, text, len);
    terminal_set_follow_output(true);
    terminal_jump_to_bottom();
}

bool terminal_spawn_shell(const char* start_dir, int rows, int cols) {
    if (g_terminalBackend) {
        terminal_shutdown_shell();
    }

    clearTerminal();
    terminal_set_follow_output(true);
    g_backendConsumed = 0;
    g_backendExitNotified = false;

    g_terminalBackend = terminal_backend_spawn(start_dir, rows, cols);
    if (!g_terminalBackend) {
        printToTerminal("[Terminal] Failed to start shell.\n");
        return false;
    }

    char msg[256];
    if (start_dir && *start_dir) {
        snprintf(msg, sizeof(msg), "[Terminal] Started shell in %s\n", start_dir);
    } else {
        snprintf(msg, sizeof(msg), "[Terminal] Started shell.\n");
    }
    printToTerminal(msg);
    return true;
}

void terminal_shutdown_shell(void) {
    if (g_terminalBackend) {
        terminal_backend_destroy(g_terminalBackend);
        g_terminalBackend = NULL;
    }
    g_backendConsumed = 0;
    g_backendExitNotified = false;
}

void terminal_tick_backend(void) {
    if (!g_terminalBackend) return;
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(g_terminalBackend->master_fd, &readfds);
    struct timeval tv = {0, 0}; // non-blocking poll
    int ready = select(g_terminalBackend->master_fd + 1, &readfds, NULL, NULL, &tv);
    if (ready > 0 && FD_ISSET(g_terminalBackend->master_fd, &readfds)) {
        terminal_backend_read_output(g_terminalBackend);
    }
    terminal_backend_poll_child(g_terminalBackend);
    terminal_flush_backend_output();
}

void terminal_resize_grid_for_pane(int width_px, int height_px) {
    if (width_px == g_lastViewportW && height_px == g_lastViewportH) return;
    g_lastViewportW = width_px;
    g_lastViewportH = height_px;
    // Use font metrics to compute cols/rows; fall back to defaults.
    TTF_Font* font = getActiveFont();
    int cellW = g_cellWidth;
    int cellH = g_cellHeight;
    if (font) {
        int h = 0, w = 0;
        if (TTF_SizeText(font, "Mg", &w, &h) == 0) {
            cellH = h;
        } else {
            cellH = TTF_FontLineSkip(font);
        }
        int mW = getTextWidth("M");
        int wW = getTextWidth("W");
        int spaceW = getTextWidth(" ");
        cellW = mW > wW ? mW : wW;
        if (spaceW > cellW) cellW = spaceW;
    }
    if (cellW <= 0) cellW = 8;
    if (cellH <= 0) cellH = 16;
    g_cellWidth = cellW;
    g_cellHeight = cellH;
    int viewCols = (cellW > 0) ? (width_px / cellW) : g_gridCols;
    int viewRows = (cellH > 0) ? (height_px / cellH) : g_gridRows;
    if (viewCols < 10) viewCols = g_gridCols;
    if (viewRows < 5) viewRows = g_gridRows;

    // Keep plenty of scrollback rows beyond the visible area.
    const int scrollbackExtra = 512;
    int desiredRows = viewRows + scrollbackExtra;
    if (desiredRows < g_gridRows) desiredRows = g_gridRows; // don't shrink and lose scrollback
    g_gridCols = viewCols;
    g_gridRows = desiredRows;
    term_grid_resize(&g_termGrid, desiredRows, viewCols);
    if (g_terminalBackend) {
        // Inform the shell of the visible size, not the scrollback buffer size.
        terminal_backend_resize(g_terminalBackend, viewRows, viewCols);
    }
}
