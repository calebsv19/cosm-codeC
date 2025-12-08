#include "terminal.h"
#include "engine/Render/render_pipeline.h"   // drawText, drawClippedText
#include "engine/Render/render_text_helpers.h"
#include "core/Clipboard/clipboard.h"
#include "ide/UI/scroll_manager.h"
#include "core/Terminal/terminal_backend.h"
#include "ide/Panes/Terminal/terminal_grid.h"
#include "engine/Render/render_font.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/select.h>
#include <SDL2/SDL_ttf.h>

static char terminalLines[MAX_TERMINAL_LINES][MAX_TERMINAL_LINE_LENGTH];
static int terminalLineCount = 0;

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
int g_gridRows = 32;
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

static void terminal_shift_lines_if_needed(void) {
    if (terminalLineCount < MAX_TERMINAL_LINES) return;
    for (int i = 1; i < MAX_TERMINAL_LINES; ++i) {
        memcpy(terminalLines[i - 1], terminalLines[i], MAX_TERMINAL_LINE_LENGTH);
    }
    terminalLineCount = MAX_TERMINAL_LINES - 1;
}

static void terminal_start_new_line(void) {
    terminal_shift_lines_if_needed();
    if (terminalLineCount < MAX_TERMINAL_LINES) {
        terminalLines[terminalLineCount][0] = '\0';
        terminalLineCount++;
    }
}

static void terminal_append_text(const char* text) {
    if (!text) return;

    if (terminalLineCount == 0) {
        terminal_start_new_line();
    }

    int currentLine = terminalLineCount - 1;
    const char* cursor = text;
    while (*cursor) {
        unsigned char c = (unsigned char)*cursor;
        if (c == '\n') {
            terminal_start_new_line();
            currentLine = terminalLineCount - 1;
        } else if (c == '\r') {
            // Carriage return: reset to start of current line.
            terminalLines[currentLine][0] = '\0';
        } else if (c == '\b' || c == 0x7f) {
            // Backspace/delete: remove last char on current line, or pull from previous.
            while (currentLine >= 0) {
                size_t len = strlen(terminalLines[currentLine]);
                if (len > 0) {
                    terminalLines[currentLine][len - 1] = '\0';
                    break;
                } else if (currentLine > 0) {
                    // Remove empty line and move up.
                    terminalLineCount = currentLine;
                    currentLine--;
                } else {
                    break;
                }
            }
        } else if (c >= 0x20 || c == '\t') {
            size_t curLen = strlen(terminalLines[currentLine]);
            if (curLen + 1 < MAX_TERMINAL_LINE_LENGTH) {
                terminalLines[currentLine][curLen] = (char)c;
                terminalLines[currentLine][curLen + 1] = '\0';
            }
        }
        cursor++;
    }

    ensure_terminal_scroll_state();
}

static void clamp_selection_position(int* line, int* column) {
    if (*line < 0) *line = 0;
    if (*line >= terminalLineCount) *line = terminalLineCount - 1;
    if (*line < 0) {
        *line = 0;
        *column = 0;
        return;
    }

    const char* text = terminalLines[*line];
    int len = text ? (int)strlen(text) : 0;
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
    terminalLineCount = 0;
    memset(terminalLines, 0, sizeof(terminalLines));
    terminal_clear_selection();
    g_backendConsumed = 0;
    g_backendExitNotified = false;
    term_grid_init(&g_termGrid, g_gridRows, g_gridCols);
}

void printToTerminal(const char* text) {
    terminal_append_text(text);
    terminal_clear_selection();
}

void clearTerminal() {
    terminalLineCount = 0;
    memset(terminalLines, 0, sizeof(terminalLines));
    terminal_clear_selection();
    ensure_terminal_scroll_state();
    g_terminalScrollState.offset_px = 0.0f;
    g_terminalScrollState.target_offset_px = 0.0f;
    term_grid_clear(&g_termGrid);
    g_lastViewportW = -1;
    g_lastViewportH = -1;
}

const char** getTerminalBuffer() {
    static const char* buffer[MAX_TERMINAL_LINES];
    for (int i = 0; i < terminalLineCount; i++) {
        buffer[i] = terminalLines[i];
    }
    return buffer;
}

int getTerminalLineCount() {
    return terminalLineCount;
}

void terminal_begin_selection(int line, int column) {
    if (terminalLineCount == 0) {
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
    if (!g_selection.selecting || terminalLineCount == 0) return;
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
    if (terminalLineCount == 0) return false;
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
        const char* text = terminalLines[line];
        if (!text) text = "";
        int from = (line == startLine) ? startCol : 0;
        int to = (line == endLine) ? endCol : (int)strlen(text);
        if (from < 0) from = 0;
        if (to < from) to = from;
        total += (size_t)(to - from);
        if (line != endLine) total++;
    }

    char* buffer = (char*)malloc(total + 1);
    if (!buffer) return false;

    size_t offset = 0;
    for (int line = startLine; line <= endLine; ++line) {
        const char* text = terminalLines[line];
        if (!text) text = "";
        int from = (line == startLine) ? startCol : 0;
        int to = (line == endLine) ? endCol : (int)strlen(text);
        if (from < 0) from = 0;
        if (to < from) to = from;
        size_t chunk = (size_t)(to - from);
        if (chunk > 0) {
            memcpy(buffer + offset, text + from, chunk);
            offset += chunk;
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

// Lightweight ANSI/OSC stripper to keep the line renderer readable.
static char* sanitize_output_chunk(const char* input, size_t len, size_t* out_len) {
    if (!input || len == 0) return NULL;
    char* out = (char*)malloc(len + 1);
    if (!out) return NULL;

    size_t w = 0;
    for (size_t i = 0; i < len; ++i) {
        unsigned char c = (unsigned char)input[i];
        if (c == 0x1b) {
            if (i + 1 < len) {
                unsigned char next = (unsigned char)input[i + 1];
                if (next == '[') {
                    // CSI: ESC [ ... terminator
                    i += 2;
                    while (i < len) {
                        unsigned char term = (unsigned char)input[i];
                        if ((term >= '@' && term <= '~') || isalpha(term)) {
                            break;
                        }
                        i++;
                    }
                    continue;
                } else if (next == ']') {
                    /* OSC: ESC ] ... BEL or ESC \ */
                    i += 2;
                    while (i < len) {
                        if ((unsigned char)input[i] == 0x07) {
                            break;
                        }
                        if ((unsigned char)input[i] == 0x1b &&
                            i + 1 < len &&
                            (unsigned char)input[i + 1] == '\\') {
                            i++;
                            break;
                        }
                        i++;
                    }
                    continue;
                } else {
                    // Other small escapes, drop next char.
                    i++;
                    continue;
                }
            }
            continue;
        }

        if (c < 0x20 && c != '\n' && c != '\t' && c != '\b') {
            continue;
        }

        out[w++] = (char)c;
    }
    out[w] = '\0';
    if (out_len) *out_len = w;
    return out;
}

// Trim trailing spaces/tabs on lines that are terminated by '\n'.
// Leaves the last line untouched if it has no trailing newline so user-typed
// spaces on the current prompt remain visible.
static char* trim_trailing_spaces_completed_lines(const char* input, size_t len, size_t* out_len) {
    if (!input || len == 0) return NULL;
    char* out = (char*)malloc(len + 1);
    if (!out) return NULL;
    size_t w = 0;
    const char* start = input;
    const char* end = input + len;
    while (start < end) {
        const char* nl = memchr(start, '\n', (size_t)(end - start));
        const char* lineEnd = nl ? nl : end;
        const char* trimEnd = lineEnd;
        bool hasNewline = (nl != NULL);
        if (hasNewline) {
            while (trimEnd > start && (trimEnd[-1] == ' ' || trimEnd[-1] == '\t')) {
                trimEnd--;
            }
        }
        size_t chunk = (size_t)(trimEnd - start);
        if (chunk > 0) {
            memcpy(out + w, start, chunk);
            w += chunk;
        }
        if (hasNewline) {
            out[w++] = '\n';
            start = nl + 1;
        } else {
            start = end;
        }
    }
    out[w] = '\0';
    if (out_len) *out_len = w;
    return out;
}

static void terminal_flush_backend_output(void) {
    if (!g_terminalBackend) return;

    size_t len = 0;
    const char* data = terminal_backend_buffer(g_terminalBackend, &len);
    if (data && len > g_backendConsumed) {
        size_t chunkLen = len - g_backendConsumed;
        size_t cleanedLen = 0;
        char* raw = (char*)malloc(chunkLen + 1);
        if (raw) {
            memcpy(raw, data + g_backendConsumed, chunkLen);
            raw[chunkLen] = '\0';
            // Feed raw bytes to emulator
            term_emulator_feed(&g_termGrid, raw, chunkLen);
            // Strip for legacy line renderer
            char* cleaned = sanitize_output_chunk(raw, chunkLen, &cleanedLen);
            if (cleaned) {
                size_t trimmedLen = 0;
                char* trimmed = trim_trailing_spaces_completed_lines(cleaned, cleanedLen, &trimmedLen);
                if (trimmed) {
                    terminal_append_text(trimmed);
                    free(trimmed);
                } else {
                    terminal_append_text(cleaned);
                }
                free(cleaned);
            }
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
}

bool terminal_spawn_shell(const char* start_dir, int rows, int cols) {
    if (g_terminalBackend) {
        terminal_shutdown_shell();
    }

    clearTerminal();
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
    term_grid_free(&g_termGrid);
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
    int cols = (cellW > 0) ? (width_px / cellW) : g_gridCols;
    int rows = (cellH > 0) ? (height_px / cellH) : g_gridRows;
    if (cols < 10) cols = g_gridCols;
    if (rows < 5) rows = g_gridRows;
    g_gridCols = cols;
    g_gridRows = rows;
    term_grid_resize(&g_termGrid, rows, cols);
    if (g_terminalBackend) {
        terminal_backend_resize(g_terminalBackend, rows, cols);
    }
}
