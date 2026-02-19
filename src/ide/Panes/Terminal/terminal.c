#include "terminal.h"
#include "core/Clipboard/clipboard.h"
#include "ide/UI/scroll_manager.h"
#include "core/Terminal/terminal_backend.h"
#include "ide/Panes/Terminal/terminal_grid.h"
#include "engine/Render/render_font.h"
#include "engine/Render/render_text_helpers.h"
#include "app/GlobalInfo/workspace_prefs.h"
#include "app/GlobalInfo/core_state.h"
#include "core/BuildSystem/build_diagnostics.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/select.h>
#include <SDL2/SDL_ttf.h>

typedef struct {
    TerminalBackend* backend;
    size_t backendConsumed;
    bool backendExitNotified;
    TermGrid grid;
    int gridRows;
    int gridCols;
    int cellWidth;
    int cellHeight;
    int lastViewportW;
    int lastViewportH;
    PaneScrollState scrollState;
    bool scrollInitialized;
    SDL_Rect scrollTrack;
    SDL_Rect scrollThumb;
    bool followOutput;
    char name[64];
    int id;
    bool inUse;
    bool isBuild;
    bool isRun;
    int historyRows;
} TerminalSession;

#define MAX_TERMINAL_SESSIONS 8
static TerminalSession g_sessions[MAX_TERMINAL_SESSIONS];
static int g_session_count = 0;
static int g_active_index = 0;
static SDL_Rect g_tab_rects[MAX_TERMINAL_SESSIONS];
static int g_tab_rect_count = 0;
static SDL_Rect g_plus_rect = {0};
static SDL_Rect g_close_rect = {0};
static int g_next_id = 1;
static int count_interactive(void) {
    int n = 0;
    for (int i = 0; i < g_session_count; ++i) {
        if (!g_sessions[i].isBuild && !g_sessions[i].isRun) n++;
    }
    return n;
}

typedef struct {
    bool selecting;
    bool hasSelection;
    int anchorLine;
    int anchorCol;
    int cursorLine;
    int cursorCol;
} TerminalSelectionState;

static TerminalSelectionState g_selection = {0};
enum {
    TERMINAL_SCROLLBACK_EXTRA_ROWS = 4000,
    TERMINAL_INITIAL_ROWS = 1024,
    TERMINAL_INITIAL_COLS = 120,
};

static const PaneScrollConfig kTerminalScrollConfig = {
    .line_height_px = TERMINAL_LINE_HEIGHT,
    .deceleration_px = 0.0f,
    .allow_negative = false,
};

static TerminalSession* active_session(void) {
    if (g_session_count == 0) return NULL;
    if (g_active_index < 0 || g_active_index >= g_session_count) g_active_index = 0;
    return &g_sessions[g_active_index];
}

static void ensure_terminal_scroll_state(void) {
    TerminalSession* s = active_session();
    if (!s) return;
    if (!s->scrollInitialized) {
        scroll_state_init(&s->scrollState, &kTerminalScrollConfig);
        s->scrollInitialized = true;
    }
}

static void terminal_feed_bytes(TerminalSession* s, const char* bytes, size_t len) {
    if (!s || !bytes || len == 0) return;
    term_emulator_feed(&s->grid, bytes, len);
    if (s->historyRows < 1) s->historyRows = 1;
    for (size_t i = 0; i < len; ++i) {
        if (bytes[i] == '\n') {
            s->historyRows++;
        }
    }
}

static int terminal_last_used_row_for_session(const TerminalSession* s) {
    if (!s || !s->grid.cells || s->grid.rows <= 0) return -1;
    for (int r = s->grid.rows - 1; r >= 0; --r) {
        for (int c = 0; c < s->grid.cols; ++c) {
            TermCell* cell = term_grid_cell((TermGrid*)&s->grid, r, c);
            char ch = cell ? (char)(cell->ch ? cell->ch : ' ') : ' ';
            if (ch != ' ') {
                return r;
            }
        }
    }
    return -1;
}

static int terminal_session_content_rows(TerminalSession* s) {
    if (!s || !s->grid.cells || s->grid.rows <= 0) return 1;
    int rows = 1;
    if (s->historyRows > rows) rows = s->historyRows;
    int lastRowUsed = terminal_last_used_row_for_session(s);
    if (lastRowUsed >= 0 && (lastRowUsed + 1) > rows) rows = lastRowUsed + 1;
    int cursorRows = s->grid.cursor_row + 1;
    if (cursorRows > rows) rows = cursorRows;
    if (rows > s->grid.rows) rows = s->grid.rows;
    return rows;
}

static void terminal_jump_to_bottom(void) {
    TerminalSession* s = active_session();
    if (!s) return;
    ensure_terminal_scroll_state();
    PaneScrollState* scroll = &s->scrollState;
    int contentRows = terminal_session_content_rows(s);
    float contentHeight = (float)s->cellHeight * (float)contentRows;
    scroll_state_set_content_height(scroll, contentHeight);
    float maxOffset = contentHeight - scroll->viewport_height_px;
    if (maxOffset < 0.0f) maxOffset = 0.0f;
    scroll->target_offset_px = maxOffset;
    scroll->offset_px = maxOffset;
}

TermGrid* terminal_active_grid(void) {
    TerminalSession* s = active_session();
    return s ? &s->grid : NULL;
}

int terminal_cell_width(void) {
    TerminalSession* s = active_session();
    return s ? s->cellWidth : 8;
}

int terminal_cell_height(void) {
    TerminalSession* s = active_session();
    return s ? s->cellHeight : TERMINAL_LINE_HEIGHT;
}

int terminal_session_count(void) {
    return g_session_count;
}

int terminal_active_index(void) {
    return g_active_index;
}

bool terminal_set_active(int index) {
    if (index < 0 || index >= g_session_count) return false;
    g_active_index = index;
    return true;
}

bool terminal_session_info(int index, const char** name, bool* isBuild, bool* isRun) {
    if (index < 0 || index >= g_session_count) return false;
    TerminalSession* s = &g_sessions[index];
    if (name) *name = s->name;
    if (isBuild) *isBuild = s->isBuild;
    if (isRun) *isRun = s->isRun;
    return true;
}

bool terminal_set_name(int index, const char* name) {
    if (index < 0 || index >= g_session_count) return false;
    TerminalSession* s = &g_sessions[index];
    if (!name) return false;
    snprintf(s->name, sizeof(s->name), "%s", name);
    return true;
}

int terminal_find_task(bool isBuild, bool isRun) {
    for (int i = 0; i < g_session_count; ++i) {
        if (g_sessions[i].isBuild == isBuild && g_sessions[i].isRun == isRun) {
            return i;
        }
    }
    return -1;
}

bool terminal_activate_task(bool isBuild, bool isRun) {
    int idx = terminal_find_task(isBuild, isRun);
    if (idx < 0) return false;
    return terminal_set_active(idx);
}

int terminal_create_interactive(const char* start_dir) {
    if (g_session_count >= MAX_TERMINAL_SESSIONS) return -1;
    int termNumber = count_interactive() + 1;
    int idx = g_session_count++;
    TerminalSession* s = &g_sessions[idx];
    memset(s, 0, sizeof(*s));
    s->gridRows = TERMINAL_INITIAL_ROWS;
    s->gridCols = TERMINAL_INITIAL_COLS;
    s->cellWidth = 8;
    s->cellHeight = TERMINAL_LINE_HEIGHT;
    s->lastViewportW = -1;
    s->lastViewportH = -1;
    s->followOutput = true;
    s->historyRows = 1;
    s->inUse = true;
    s->id = g_next_id++;
    snprintf(s->name, sizeof(s->name), "Term %d", termNumber);
    term_grid_init(&s->grid, s->gridRows, s->gridCols);
    g_active_index = idx;
    terminal_spawn_shell(start_dir, 0, 0);
    return idx;
}

void terminal_close_interactive(int index) {
    if (index < 0 || index >= g_session_count) return;
    // Only allow closing non-zero for now to keep at least one session alive.
    if (g_session_count <= 1) return;
    terminal_set_active(index);
    terminal_shutdown_shell();
    term_grid_free(&g_sessions[index].grid);
    // Shift sessions down.
    for (int i = index + 1; i < g_session_count; ++i) {
        g_sessions[i - 1] = g_sessions[i];
    }
    g_session_count--;
    if (g_active_index >= g_session_count) g_active_index = g_session_count - 1;
    if (g_active_index < 0) g_active_index = 0;
}

void terminal_set_tab_rect(int index, SDL_Rect rect) {
    if (index < 0 || index >= MAX_TERMINAL_SESSIONS) return;
    g_tab_rects[index] = rect;
    if (index + 1 > g_tab_rect_count) g_tab_rect_count = index + 1;
}

int terminal_tab_hit(int x, int y) {
    for (int i = 0; i < g_tab_rect_count; ++i) {
        SDL_Rect r = g_tab_rects[i];
        if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h) {
            return i;
        }
    }
    return -1;
}

bool terminal_plus_hit(int x, int y) {
    SDL_Rect r = g_plus_rect;
    return (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h);
}

void terminal_reset_tab_rects(void) {
    g_tab_rect_count = 0;
}

void terminal_set_plus_rect(SDL_Rect rect) {
    g_plus_rect = rect;
}

void terminal_set_close_rect(SDL_Rect rect) {
    g_close_rect = rect;
}

bool terminal_close_hit(int x, int y) {
    SDL_Rect r = g_close_rect;
    return (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h);
}

bool terminal_active_is_task(void) {
    TerminalSession* s = active_session();
    if (!s) return false;
    return s->isBuild || s->isRun;
}

bool terminal_close_active_interactive(void) {
    if (terminal_active_is_task()) return false;
    if (count_interactive() <= 1) return false;
    terminal_close_interactive(g_active_index);
    return true;
}

void terminal_append_to_session(int index, const char* text) {
    if (!text) return;
    if (index < 0 || index >= g_session_count) return;
    TerminalSession* s = &g_sessions[index];
    terminal_feed_bytes(s, text, strlen(text));
}

void terminal_clear_session(int index) {
    if (index < 0 || index >= g_session_count) return;
    TerminalSession* s = &g_sessions[index];
    term_grid_clear(&s->grid);
    if (!s->scrollInitialized) {
        scroll_state_init(&s->scrollState, &kTerminalScrollConfig);
        s->scrollInitialized = true;
    }
    s->scrollState.offset_px = 0.0f;
    s->scrollState.target_offset_px = 0.0f;
    s->historyRows = 1;
}

int terminal_line_length(int row, bool trim_trailing) {
    TerminalSession* s = active_session();
    if (!s || !s->grid.cells || row < 0 || row >= s->grid.rows) return 0;
    int lastNonSpace = -1;
    for (int c = 0; c < s->grid.cols; ++c) {
        TermCell* cell = term_grid_cell(&s->grid, row, c);
        char ch = cell ? (char)(cell->ch ? cell->ch : ' ') : ' ';
        if (ch != ' ' && ch != '\0') {
            lastNonSpace = c;
        }
    }
    if (!trim_trailing) return s->grid.cols;
    return (lastNonSpace >= 0) ? lastNonSpace + 1 : 0;
}

int terminal_line_to_string(int row, char* out, int cap, bool trim_trailing) {
    if (!out || cap <= 0) return 0;
    TerminalSession* s = active_session();
    if (!s || !s->grid.cells || row < 0 || row >= s->grid.rows) {
        out[0] = '\0';
        return 0;
    }
    int len = terminal_line_length(row, trim_trailing);
    if (len > cap - 1) len = cap - 1;
    for (int c = 0; c < len; ++c) {
        TermCell* cell = term_grid_cell(&s->grid, row, c);
        char ch = cell ? (char)(cell->ch ? cell->ch : ' ') : ' ';
        out[c] = (ch >= 0x20 && ch < 0x7F) ? ch : ' ';
    }
    out[len] = '\0';
    return len;
}

int terminal_last_used_row(void) {
    TerminalSession* s = active_session();
    if (!s || !s->grid.cells || s->grid.rows <= 0) return -1;
    for (int r = s->grid.rows - 1; r >= 0; --r) {
        for (int c = 0; c < s->grid.cols; ++c) {
            TermCell* cell = term_grid_cell(&s->grid, r, c);
            char ch = cell ? (char)(cell->ch ? cell->ch : ' ') : ' ';
            if (ch != ' ') {
                return r;
            }
        }
    }
    return -1;
}

int terminal_content_rows(void) {
    TerminalSession* s = active_session();
    return terminal_session_content_rows(s);
}

static void clamp_selection_position(int* line, int* column) {
    TerminalSession* s = active_session();
    if (!s) { *line = 0; *column = 0; return; }
    if (*line < 0) *line = 0;
    if (*line >= s->grid.rows) *line = s->grid.rows - 1;
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

static int terminal_create_task(const char* name, bool isBuild, bool isRun);

void initTerminal() {
    terminal_clear_selection();
    g_session_count = 0;
    g_active_index = 0;
    int buildIdx = terminal_create_task("Build", true, false);
    int runIdx = terminal_create_task("Run", false, true);
    (void)buildIdx; (void)runIdx;
    int interIdx = terminal_create_interactive(getWorkspacePath());
    if (interIdx >= 0) terminal_set_active(interIdx);
}

void printToTerminal(const char* text) {
    if (!text) return;
    size_t len = strlen(text);
    TerminalSession* s = active_session();
    if (!s) return;
    terminal_feed_bytes(s, text, len);
}

void clearTerminal() {
    terminal_clear_selection();
    TerminalSession* s = active_session();
    if (!s) return;
    ensure_terminal_scroll_state();
    s->scrollState.offset_px = 0.0f;
    s->scrollState.target_offset_px = 0.0f;
    term_grid_clear(&s->grid);
    s->historyRows = 1;
    s->lastViewportW = -1;
    s->lastViewportH = -1;
}

void terminal_begin_selection(int line, int column) {
    TerminalSession* s = active_session();
    if (!s || !s->grid.cells || s->grid.rows == 0) {
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
    TerminalSession* s = active_session();
    if (!s || !g_selection.selecting || s->grid.rows == 0) return;
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
    TerminalSession* s = active_session();
    if (!s || !s->grid.cells || s->grid.rows == 0) return false;
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
    TerminalSession* s = active_session();
    return s ? &s->scrollState : NULL;
}

void terminal_set_scroll_track(const SDL_Rect* track, const SDL_Rect* thumb) {
    TerminalSession* s = active_session();
    if (!s) return;
    if (track) s->scrollTrack = *track; else s->scrollTrack = (SDL_Rect){0};
    if (thumb) s->scrollThumb = *thumb; else s->scrollThumb = (SDL_Rect){0};
}

void terminal_get_scroll_track(SDL_Rect* track, SDL_Rect* thumb) {
    TerminalSession* s = active_session();
    if (!s) return;
    if (track) *track = s->scrollTrack;
    if (thumb) *thumb = s->scrollThumb;
}

void terminal_set_follow_output(bool follow) {
    ensure_terminal_scroll_state();
    TerminalSession* s = active_session();
    if (s) s->followOutput = follow;
}

bool terminal_is_following_output(void) {
    ensure_terminal_scroll_state();
    TerminalSession* s = active_session();
    return s ? s->followOutput : false;
}

static void terminal_flush_backend_output(void) {
    TerminalSession* s = active_session();
    if (!s || !s->backend) return;

    size_t len = 0;
    const char* data = terminal_backend_buffer(s->backend, &len);
    if (data && len > s->backendConsumed) {
        size_t chunkLen = len - s->backendConsumed;
        char* raw = (char*)malloc(chunkLen + 1);
        if (raw) {
            memcpy(raw, data + s->backendConsumed, chunkLen);
            raw[chunkLen] = '\0';
            // Feed raw bytes to emulator
            terminal_feed_bytes(s, raw, chunkLen);
            if (s->isBuild) {
                build_diagnostics_feed_chunk(raw, chunkLen);
            }
            free(raw);
        }
        s->backendConsumed = len;
    }

    if (s->backend->dead && !s->backendExitNotified) {
        printToTerminal("[Terminal] Shell process exited.\n");
        s->backendExitNotified = true;
    }
}

void terminal_send_text(const char* text, size_t len) {
    if (!text || len == 0) return;
    TerminalSession* s = active_session();
    if (!s || !s->backend || s->backend->dead) return;
    terminal_backend_send_input(s->backend, text, len);
    terminal_set_follow_output(true);
    terminal_jump_to_bottom();
}

bool terminal_spawn_shell(const char* start_dir, int rows, int cols) {
    TerminalSession* s = active_session();
    if (!s) return false;
    if (s->backend) {
        terminal_shutdown_shell();
    }

    clearTerminal();
    terminal_set_follow_output(true);
    s->backendConsumed = 0;
    s->backendExitNotified = false;

    s->backend = terminal_backend_spawn(start_dir, rows, cols);
    if (!s->backend) {
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
    TerminalSession* s = active_session();
    if (!s) return;
    if (s->backend) {
        terminal_backend_destroy(s->backend);
        s->backend = NULL;
    }
    s->backendConsumed = 0;
    s->backendExitNotified = false;
}

void terminal_tick_backend(void) {
    TerminalSession* s = active_session();
    if (!s || !s->backend) return;
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(s->backend->master_fd, &readfds);
    struct timeval tv = {0, 0}; // non-blocking poll
    int ready = select(s->backend->master_fd + 1, &readfds, NULL, NULL, &tv);
    if (ready > 0 && FD_ISSET(s->backend->master_fd, &readfds)) {
        terminal_backend_read_output(s->backend);
    }
    terminal_backend_poll_child(s->backend);
    terminal_flush_backend_output();
}

void terminal_resize_grid_for_pane(int width_px, int height_px) {
    TerminalSession* s = active_session();
    if (!s) return;
    if (width_px == s->lastViewportW && height_px == s->lastViewportH) return;
    s->lastViewportW = width_px;
    s->lastViewportH = height_px;
    // Use font metrics to compute cols/rows; fall back to defaults.
    TTF_Font* font = getTerminalFont();
    int cellW = s->cellWidth;
    int cellH = s->cellHeight;
    if (font) {
        int h = 0, w = 0;
        if (TTF_SizeText(font, "Mg", &w, &h) == 0) {
            cellH = TTF_FontLineSkip(font);
            if (cellH <= 0) cellH = h;
        } else {
            cellH = TTF_FontLineSkip(font);
        }
        int minx = 0, maxx = 0, miny = 0, maxy = 0, advance = 0;
        if (TTF_GlyphMetrics(font, 'M', &minx, &maxx, &miny, &maxy, &advance) == 0 && advance > 0) {
            cellW = advance;
        } else {
            int mW = getTextWidthWithFont("M", font);
            int wW = getTextWidthWithFont("W", font);
            int spaceW = getTextWidthWithFont(" ", font);
            cellW = mW > wW ? mW : wW;
            if (spaceW > cellW) cellW = spaceW;
        }
    }
    if (cellW <= 0) cellW = 8;
    if (cellH <= 0) cellH = 16;
    int viewCols = (cellW > 0) ? (width_px / cellW) : s->gridCols;
    int viewRows = (cellH > 0) ? (height_px / cellH) : s->gridRows;
    if (viewCols < 10) viewCols = s->gridCols;
    if (viewRows < 5) viewRows = s->gridRows;

    // Keep plenty of scrollback rows beyond the visible area.
    const int scrollbackExtra = TERMINAL_SCROLLBACK_EXTRA_ROWS;
    int desiredRows = viewRows + scrollbackExtra;
    if (desiredRows < s->gridRows) desiredRows = s->gridRows; // don't shrink and lose scrollback
    s->gridCols = viewCols;
    s->gridRows = desiredRows;
    s->cellWidth = cellW;
    s->cellHeight = cellH;
    term_grid_resize(&s->grid, desiredRows, viewCols);
    if (s->backend) {
        // Inform the shell of the visible size, not the scrollback buffer size.
        terminal_backend_resize(s->backend, viewRows, viewCols);
    }
}
static int terminal_create_task(const char* name, bool isBuild, bool isRun) {
    if (g_session_count >= MAX_TERMINAL_SESSIONS) return -1;
    int idx = g_session_count++;
    TerminalSession* s = &g_sessions[idx];
    memset(s, 0, sizeof(*s));
    s->gridRows = TERMINAL_INITIAL_ROWS;
    s->gridCols = TERMINAL_INITIAL_COLS;
    s->cellWidth = 8;
    s->cellHeight = TERMINAL_LINE_HEIGHT;
    s->lastViewportW = -1;
    s->lastViewportH = -1;
    s->followOutput = true;
    s->historyRows = 1;
    s->inUse = true;
    s->isBuild = isBuild;
    s->isRun = isRun;
    s->id = g_next_id++;
    snprintf(s->name, sizeof(s->name), "%s", name ? name : (isBuild ? "Build" : (isRun ? "Run" : "Task")));
    term_grid_init(&s->grid, s->gridRows, s->gridCols);
    return idx;
}
