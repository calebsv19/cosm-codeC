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
#include "core/Ipc/ide_ipc_server.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
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
    int lastBackendRows;
    int lastBackendCols;
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
    TerminalVisibleBuffer visibleModel;
    TerminalScrollbackRing scrollbackModel;
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
static bool g_terminal_safe_paste_enabled = true;
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
    TERMINAL_SCROLLBACK_EXTRA_ROWS = 50000,
    TERMINAL_SCROLLBACK_EXTRA_ROWS_MIN = 2000,
    TERMINAL_SCROLLBACK_EXTRA_ROWS_MAX = 200000,
    TERMINAL_INITIAL_ROWS = 1024,
    TERMINAL_INITIAL_COLS = 120,
};
static int g_terminal_scrollback_extra_rows = TERMINAL_SCROLLBACK_EXTRA_ROWS;
static bool g_terminal_debug_model_enabled = false;
static bool g_terminal_debug_overlay_enabled = false;
static bool g_terminal_debug_pipeline_enabled = false;
static bool g_terminal_enable_alternate_screen = true;
static unsigned long long g_terminal_pipeline_bytes = 0ull;
static unsigned long long g_terminal_pipeline_emulator_feed_calls = 0ull;
static unsigned long long g_terminal_pipeline_legacy_feed_calls = 0ull;

static const PaneScrollConfig kTerminalScrollConfig = {
    .line_height_px = TERMINAL_LINE_HEIGHT,
    .deceleration_px = 0.0f,
    .allow_negative = false,
};

static void terminal_model_log(const char* fmt, ...) {
    if (!g_terminal_debug_model_enabled || !fmt) return;
    va_list args;
    va_start(args, fmt);
    printf("[TerminalModel] ");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
}

static void terminal_pipeline_log(const char* fmt, ...) {
    if (!g_terminal_debug_pipeline_enabled || !fmt) return;
    va_list args;
    va_start(args, fmt);
    printf("[TerminalPipeline] ");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
}

static TerminalSession* active_session(void) {
    if (g_session_count == 0) return NULL;
    if (g_active_index < 0 || g_active_index >= g_session_count) g_active_index = 0;
    return &g_sessions[g_active_index];
}

static int terminal_scrollback_extra_rows(void) {
    if (g_terminal_scrollback_extra_rows > 0) return g_terminal_scrollback_extra_rows;
    return TERMINAL_SCROLLBACK_EXTRA_ROWS;
}

static int terminal_clamp_rows(const TermGrid* grid, int rows) {
    if (!grid || grid->rows <= 0) return 1;
    if (rows < 1) rows = 1;
    if (rows > grid->rows) rows = grid->rows;
    return rows;
}

static int terminal_session_content_rows(const TerminalSession* s) {
    if (!s) return 1;
    int viewportRows = s->grid.viewport_rows;
    if (viewportRows < 1 || viewportRows > s->grid.rows) viewportRows = s->grid.rows;
    if (s->grid.using_alternate) {
        return viewportRows > 0 ? viewportRows : 1;
    }
    int rows = term_grid_scrollback_count(&s->grid) + viewportRows;
    return rows > 0 ? rows : 1;
}

static int terminal_viewport_start_row(const TerminalSession* s) {
    if (!s) return 0;
    int viewportRows = s->grid.viewport_rows;
    if (viewportRows < 1 || viewportRows > s->grid.rows) viewportRows = s->grid.rows;
    int usedRows = s->grid.used_rows;
    if (usedRows < 1) usedRows = 1;
    if (usedRows > s->grid.rows) usedRows = s->grid.rows;
    int start = usedRows - viewportRows;
    if (start < 0) start = 0;
    if (start >= s->grid.rows) start = s->grid.rows - 1;
    return start;
}

static void terminal_rebuild_session_model(TerminalSession* s, const char* reason) {
    if (!s) return;
    TermGrid* grid = &s->grid;

    TerminalVisibleBuffer visible = {0};
    visible.cells = NULL;
    visible.rows = terminal_clamp_rows(grid, grid->viewport_rows > 0 ? grid->viewport_rows : grid->rows);
    visible.cols = grid->cols > 0 ? grid->cols : 1;
    if (grid->using_alternate) {
        visible.cursor_row = grid->cursor_row;
        if (visible.cursor_row < 0) visible.cursor_row = 0;
        if (visible.cursor_row >= visible.rows) visible.cursor_row = visible.rows - 1;
        visible.cursor_col = grid->cursor_col;
        if (visible.cursor_col < 0) visible.cursor_col = 0;
        if (visible.cursor_col >= visible.cols) visible.cursor_col = visible.cols - 1;
    } else {
        visible.cursor_row = grid->cursor_row;
        if (visible.cursor_row < 0) visible.cursor_row = 0;
        if (visible.cursor_row >= terminal_session_content_rows(s)) {
            visible.cursor_row = terminal_session_content_rows(s) - 1;
        }
        visible.cursor_col = grid->cursor_col;
        if (visible.cursor_col < 0) visible.cursor_col = 0;
        if (visible.cursor_col >= visible.cols) visible.cursor_col = visible.cols - 1;
    }
    visible.using_alternate = grid->using_alternate != 0;

    int projectedRows = terminal_session_content_rows(s);
    int scrollbackRows = visible.using_alternate ? 0 : term_grid_scrollback_count(grid);

    TerminalScrollbackRing scrollback = {0};
    scrollback.row_count = visible.using_alternate ? 0 : scrollbackRows;
    scrollback.cap_rows = grid->scrollback_cap_rows;

    s->visibleModel = visible;
    s->scrollbackModel = scrollback;

    if (g_terminal_debug_model_enabled) {
        terminal_model_log("projection reason=%s mode=%s viewport=%dx%d cursor=%d,%d scrollback=%d content=%d",
                           reason ? reason : "unknown",
                           visible.using_alternate ? "alternate" : "primary",
                           visible.rows,
                           visible.cols,
                           visible.cursor_row,
                           visible.cursor_col,
                           scrollback.row_count,
                           projectedRows);
    }
    terminal_pipeline_log("snapshot reason=%s mode=%s cursor=%d,%d viewport=%dx%d projected=%d scrollback=%d",
                          reason ? reason : "unknown",
                          visible.using_alternate ? "alternate" : "primary",
                          visible.cursor_row,
                          visible.cursor_col,
                          visible.rows,
                          visible.cols,
                          projectedRows,
                          scrollback.row_count);
    terminal_pipeline_log("scrollback commits=%llu rows=%d cap=%d drops=%llu",
                          term_grid_scrollback_commit_count(grid),
                          term_grid_scrollback_count(grid),
                          grid->scrollback_cap_rows,
                          term_grid_scrollback_drop_count(grid));
    terminal_pipeline_log("alt enters=%llu exits=%llu ignored=%llu",
                          term_grid_alt_enter_count(grid),
                          term_grid_alt_exit_count(grid),
                          term_grid_alt_ignored_count(grid));
}

static void terminal_validate_invariants(const TerminalSession* s) {
    if (!s) return;
    const TermGrid* grid = &s->grid;
    if (!grid->cells || grid->rows <= 0 || grid->cols <= 0) return;
    assert(grid->cursor_row >= 0 && grid->cursor_row < grid->rows);
    assert(grid->cursor_col >= 0 && grid->cursor_col < grid->cols);

    int projectedRows = terminal_session_content_rows(s);
    int viewportRows = s->visibleModel.rows;
    int scrollbackRows = s->scrollbackModel.row_count;
    if (!s->visibleModel.using_alternate) {
        assert(projectedRows == scrollbackRows + viewportRows);
    } else {
        assert(scrollbackRows == 0);
    }

    if (g_selection.hasSelection) {
        if (g_selection.anchorLine < 0 || g_selection.anchorLine >= projectedRows ||
            g_selection.cursorLine < 0 || g_selection.cursorLine >= projectedRows) {
            g_selection = (TerminalSelectionState){0};
        }
    }
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
    g_terminal_pipeline_bytes += (unsigned long long)len;
    g_terminal_pipeline_emulator_feed_calls++;
    unsigned long long commits_before = term_grid_scrollback_commit_count(&s->grid);
    bool wasAlternate = s->grid.using_alternate != 0;
    term_emulator_feed(&s->grid, bytes, len);
    bool nowAlternate = s->grid.using_alternate != 0;
    unsigned long long commits_after = term_grid_scrollback_commit_count(&s->grid);
    if (wasAlternate && nowAlternate) {
        assert(commits_before == commits_after);
    }
    terminal_pipeline_log("feed bytes=%zu emulator_calls=%llu legacy_calls=%llu",
                          len,
                          g_terminal_pipeline_emulator_feed_calls,
                          g_terminal_pipeline_legacy_feed_calls);
    assert(g_terminal_pipeline_legacy_feed_calls == 0ull);
    if (wasAlternate != nowAlternate) {
        terminal_model_log("mode_switch from=%s to=%s",
                           wasAlternate ? "alternate" : "primary",
                           nowAlternate ? "alternate" : "primary");
    }
    terminal_rebuild_session_model(s, "feed");
    terminal_validate_invariants(s);
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

bool terminal_get_visible_buffer(TerminalVisibleBuffer* out) {
    TerminalSession* s = active_session();
    if (!s || !out) return false;
    terminal_rebuild_session_model(s, "visible");
    *out = s->visibleModel;
    return true;
}

bool terminal_get_scrollback_ring(TerminalScrollbackRing* out) {
    TerminalSession* s = active_session();
    if (!s || !out) return false;
    terminal_rebuild_session_model(s, "scrollback");
    *out = s->scrollbackModel;
    return true;
}

int terminal_projection_row_count(void) {
    TerminalSession* s = active_session();
    if (!s) return 1;
    terminal_rebuild_session_model(s, "projection_count");
    return terminal_session_content_rows(s);
}

bool terminal_projection_get_row(int index, TerminalProjectionRow* out_row) {
    TerminalSession* s = active_session();
    if (!s || !out_row) return false;
    int rows = terminal_projection_row_count();
    if (index < 0 || index >= rows) return false;
    out_row->projected_row = index;
    out_row->from_scrollback = (!s->grid.using_alternate && index < s->scrollbackModel.row_count);
    if (out_row->from_scrollback) {
        out_row->grid_row = -1;
    } else {
        int local = index - s->scrollbackModel.row_count;
        if (local < 0) local = 0;
        out_row->grid_row = terminal_viewport_start_row(s) + local;
        if (out_row->grid_row < 0) out_row->grid_row = 0;
        if (out_row->grid_row >= s->grid.rows) out_row->grid_row = s->grid.rows - 1;
    }
    return true;
}

const TermCell* terminal_projection_rowcol_to_cell(int row, int col) {
    TerminalSession* s = active_session();
    if (!s) return NULL;
    TerminalProjectionRow pr = {0};
    if (!terminal_projection_get_row(row, &pr)) return NULL;
    if (col < 0 || col >= s->grid.cols) return NULL;
    if (pr.from_scrollback) {
        const TermCell* rowCells = term_grid_scrollback_row(&s->grid, pr.projected_row);
        if (!rowCells) return NULL;
        return &rowCells[col];
    }
    return term_grid_cell(&s->grid, pr.grid_row, col);
}

bool terminal_projection_rowcol_to_grid(int row, int col, int* out_grid_row, int* out_grid_col) {
    TerminalSession* s = active_session();
    if (!s) return false;
    TerminalProjectionRow pr = {0};
    if (!terminal_projection_get_row(row, &pr)) return false;
    int mappedCol = col;
    if (mappedCol < 0) mappedCol = 0;
    if (mappedCol >= s->grid.cols) mappedCol = s->grid.cols - 1;
    if (out_grid_row) *out_grid_row = pr.grid_row;
    if (out_grid_col) *out_grid_col = mappedCol;
    return true;
}

bool terminal_get_debug_stats(TerminalDebugStats* out) {
    if (!out) return false;
    TerminalSession* s = active_session();
    if (!s) return false;
    terminal_rebuild_session_model(s, "debug_stats");
    out->using_alternate = s->visibleModel.using_alternate;
    out->cursor_row = s->visibleModel.cursor_row;
    out->cursor_col = s->visibleModel.cursor_col;
    out->viewport_rows = s->visibleModel.rows;
    out->viewport_cols = s->visibleModel.cols;
    out->scrollback_rows = s->scrollbackModel.row_count;
    out->projected_rows = terminal_session_content_rows(s);
    return true;
}

bool terminal_debug_overlay_enabled(void) {
    return g_terminal_debug_overlay_enabled;
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
    TerminalSession* s = active_session();
    if (s) {
        terminal_rebuild_session_model(s, "set_active");
        terminal_validate_invariants(s);
    }
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
    s->lastBackendRows = -1;
    s->lastBackendCols = -1;
    s->followOutput = true;
    s->inUse = true;
    s->id = g_next_id++;
    snprintf(s->name, sizeof(s->name), "Term %d", termNumber);
    term_grid_init(&s->grid, s->gridRows, s->gridCols);
    term_grid_set_scrollback_cap(&s->grid, terminal_scrollback_extra_rows());
    term_grid_set_alternate_screen_enabled(&s->grid, g_terminal_enable_alternate_screen ? 1 : 0);
    terminal_rebuild_session_model(s, "create_interactive");
    terminal_validate_invariants(s);
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
    terminal_rebuild_session_model(s, "clear_session");
    terminal_validate_invariants(s);
}

int terminal_line_length(int row, bool trim_trailing) {
    TerminalSession* s = active_session();
    if (!s) return 0;
    TerminalProjectionRow pr = {0};
    if (!terminal_projection_get_row(row, &pr)) return 0;
    int cols = s->grid.cols;
    if (cols <= 0) return 0;
    int len = cols;
    while (len > 0) {
        const TermCell* cell = terminal_projection_rowcol_to_cell(row, len - 1);
        uint32_t ch = cell ? cell->ch : (uint32_t)' ';
        if (ch == 0u) ch = (uint32_t)' ';
        if (ch != (uint32_t)' ') break;
        len--;
    }
    if (!trim_trailing) return len;
    return len;
}

static int terminal_encode_codepoint_utf8(uint32_t cp, char out[4]) {
    if (cp <= 0x7Fu) {
        out[0] = (char)cp;
        return 1;
    }
    if (cp <= 0x7FFu) {
        out[0] = (char)(0xC0u | ((cp >> 6) & 0x1Fu));
        out[1] = (char)(0x80u | (cp & 0x3Fu));
        return 2;
    }
    if (cp <= 0xFFFFu) {
        out[0] = (char)(0xE0u | ((cp >> 12) & 0x0Fu));
        out[1] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
        out[2] = (char)(0x80u | (cp & 0x3Fu));
        return 3;
    }
    if (cp <= 0x10FFFFu) {
        out[0] = (char)(0xF0u | ((cp >> 18) & 0x07u));
        out[1] = (char)(0x80u | ((cp >> 12) & 0x3Fu));
        out[2] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
        out[3] = (char)(0x80u | (cp & 0x3Fu));
        return 4;
    }
    out[0] = '?';
    return 1;
}

int terminal_line_to_string(int row, char* out, int cap, bool trim_trailing) {
    if (!out || cap <= 0) return 0;
    TerminalSession* s = active_session();
    if (!s) {
        out[0] = '\0';
        return 0;
    }
    TerminalProjectionRow pr = {0};
    if (!terminal_projection_get_row(row, &pr)) {
        out[0] = '\0';
        return 0;
    }
    int cols = s->grid.cols;
    if (cols <= 0) {
        out[0] = '\0';
        return 0;
    }
    int limitCols = cols;
    if (trim_trailing) {
        while (limitCols > 0) {
            const TermCell* cell = terminal_projection_rowcol_to_cell(row, limitCols - 1);
            uint32_t ch = cell ? cell->ch : (uint32_t)' ';
            if (ch == 0u) ch = (uint32_t)' ';
            if (ch != (uint32_t)' ') break;
            limitCols--;
        }
    } else {
        while (limitCols > 0) {
            const TermCell* cell = terminal_projection_rowcol_to_cell(row, limitCols - 1);
            uint32_t ch = cell ? cell->ch : (uint32_t)' ';
            if (ch == 0u) ch = (uint32_t)' ';
            if (ch != (uint32_t)' ') break;
            limitCols--;
        }
    }

    int outLen = 0;
    for (int c = 0; c < limitCols; ++c) {
        const TermCell* cell = terminal_projection_rowcol_to_cell(row, c);
        uint32_t cp = cell ? cell->ch : (uint32_t)' ';
        if (cp == 0u) cp = (uint32_t)' ';
        if (cp < 0x20u || cp == 0x7Fu) cp = (uint32_t)' ';
        char enc[4];
        int encLen = terminal_encode_codepoint_utf8(cp, enc);
        if (outLen + encLen >= cap) break;
        for (int i = 0; i < encLen; ++i) {
            out[outLen++] = enc[i];
        }
    }
    out[outLen] = '\0';
    return outLen;
}

int terminal_last_used_row(void) {
    return terminal_projection_row_count() - 1;
}

int terminal_content_rows(void) {
    return terminal_projection_row_count();
}

static void clamp_selection_position(int* line, int* column) {
    int totalRows = terminal_projection_row_count();
    if (totalRows < 1) totalRows = 1;
    if (*line < 0) *line = 0;
    if (*line >= totalRows) *line = totalRows - 1;
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
    const char* envRows = getenv("IDE_TERMINAL_SCROLLBACK_ROWS");
    if (envRows && envRows[0]) {
        int parsed = atoi(envRows);
        if (parsed >= TERMINAL_SCROLLBACK_EXTRA_ROWS_MIN &&
            parsed <= TERMINAL_SCROLLBACK_EXTRA_ROWS_MAX) {
            g_terminal_scrollback_extra_rows = parsed;
        } else {
            g_terminal_scrollback_extra_rows = TERMINAL_SCROLLBACK_EXTRA_ROWS;
        }
    } else {
        g_terminal_scrollback_extra_rows = TERMINAL_SCROLLBACK_EXTRA_ROWS;
    }
    g_terminal_debug_model_enabled = false;
    g_terminal_debug_overlay_enabled = false;
    const char* modelDebug = getenv("IDE_TERMINAL_DEBUG_MODEL");
    if (modelDebug && modelDebug[0] && strcmp(modelDebug, "0") != 0) {
        g_terminal_debug_model_enabled = true;
    }
    const char* overlayDebug = getenv("IDE_TERMINAL_DEBUG_OVERLAY");
    if (overlayDebug && overlayDebug[0] && strcmp(overlayDebug, "0") != 0) {
        g_terminal_debug_overlay_enabled = true;
    }
    const char* pipelineDebug = getenv("IDE_TERMINAL_DEBUG_PIPELINE");
    if (pipelineDebug && pipelineDebug[0] && strcmp(pipelineDebug, "0") != 0) {
        g_terminal_debug_pipeline_enabled = true;
    }
    const char* altScreen = getenv("IDE_TERMINAL_ENABLE_ALT_SCREEN");
    if (altScreen && altScreen[0] && strcmp(altScreen, "0") != 0) {
        g_terminal_enable_alternate_screen = true;
    } else {
        // Default to true terminal behavior.
        g_terminal_enable_alternate_screen = true;
    }
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
    s->lastViewportW = -1;
    s->lastViewportH = -1;
    s->lastBackendRows = -1;
    s->lastBackendCols = -1;
    terminal_rebuild_session_model(s, "clear");
    terminal_validate_invariants(s);
}

void terminal_begin_selection(int line, int column) {
    TerminalSession* s = active_session();
    if (!s || !s->grid.cells || terminal_projection_row_count() <= 0) {
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
    if (!s || !g_selection.selecting || terminal_projection_row_count() <= 0) return;
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
    if (!s || !s->grid.cells || terminal_projection_row_count() <= 0) return false;
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

void terminal_set_safe_paste_enabled(bool enabled) {
    g_terminal_safe_paste_enabled = enabled;
}

bool terminal_safe_paste_enabled(void) {
    return g_terminal_safe_paste_enabled;
}

void terminal_toggle_safe_paste_enabled(void) {
    g_terminal_safe_paste_enabled = !g_terminal_safe_paste_enabled;
}

static bool terminal_flush_backend_output(void) {
    bool changed = false;
    TerminalSession* s = active_session();
    if (!s || !s->backend) return false;

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
            changed = true;
        }
        s->backendConsumed = len;
    }

    if (s->backend->dead && !s->backendExitNotified) {
        printToTerminal("[Terminal] Shell process exited.\n");
        s->backendExitNotified = true;
        changed = true;
    }

    return changed;
}

void terminal_send_text(const char* text, size_t len) {
    if (!text || len == 0) return;
    TerminalSession* s = active_session();
    if (!s || !s->backend || s->backend->dead) return;
    terminal_backend_send_input(s->backend, text, len);
    terminal_set_follow_output(true);
    terminal_jump_to_bottom();
}

void terminal_handle_dropped_path(const char* path) {
    if (!path || !path[0]) return;

    // Single-quote shell escaping: abc'd -> 'abc'\''d'
    size_t len = strlen(path);
    size_t extra = 0;
    for (size_t i = 0; i < len; ++i) {
        if (path[i] == '\'') extra += 3;
    }
    size_t outCap = len + extra + 4; // quotes + trailing space + NUL
    char* escaped = (char*)malloc(outCap);
    if (!escaped) return;

    size_t w = 0;
    escaped[w++] = '\'';
    for (size_t i = 0; i < len; ++i) {
        if (path[i] == '\'') {
            escaped[w++] = '\'';
            escaped[w++] = '\\';
            escaped[w++] = '\'';
            escaped[w++] = '\'';
        } else {
            escaped[w++] = path[i];
        }
    }
    escaped[w++] = '\'';
    escaped[w++] = ' ';
    escaped[w] = '\0';

    terminal_send_text(escaped, w);
    free(escaped);
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

    s->backend = terminal_backend_spawn(start_dir, rows, cols,
                                        ide_ipc_socket_path(),
                                        getWorkspacePath(),
                                        ide_ipc_auth_token());
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

bool terminal_tick_backend(void) {
    bool changed = false;
    TerminalSession* s = active_session();
    if (!s || !s->backend) return false;
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(s->backend->master_fd, &readfds);
    struct timeval tv = {0, 0}; // non-blocking poll
    int ready = select(s->backend->master_fd + 1, &readfds, NULL, NULL, &tv);
    if (ready > 0 && FD_ISSET(s->backend->master_fd, &readfds)) {
        if (terminal_backend_read_output(s->backend) > 0) {
            changed = true;
        }
    }
    terminal_backend_poll_child(s->backend);
    if (terminal_flush_backend_output()) {
        changed = true;
    }
    return changed;
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

    int oldViewportRows = s->grid.viewport_rows;
    int oldViewportCols = s->grid.viewport_cols;
    int oldGridRows = s->grid.rows;
    int oldGridCols = s->grid.cols;
    int oldCursorRow = s->grid.cursor_row;
    int oldCursorCol = s->grid.cursor_col;
    int oldScrollRows = term_grid_scrollback_count(&s->grid);
    unsigned long long oldScrollCommits = term_grid_scrollback_commit_count(&s->grid);

    int desiredRows = viewRows;
    if (desiredRows < 1) desiredRows = 1;
    bool gridSizeChanged = (s->gridCols != viewCols) || (s->gridRows != desiredRows);
    bool viewportChanged = (s->grid.viewport_rows != viewRows) || (s->grid.viewport_cols != viewCols);

    s->gridCols = viewCols;
    s->gridRows = desiredRows;
    s->cellWidth = cellW;
    s->cellHeight = cellH;

    if (gridSizeChanged) {
        term_grid_resize(&s->grid, desiredRows, viewCols);
    }
    term_grid_set_scrollback_cap(&s->grid, terminal_scrollback_extra_rows());
    if (gridSizeChanged || viewportChanged) {
        term_grid_set_viewport_size(&s->grid, viewRows, viewCols);
        terminal_rebuild_session_model(s, "resize");
        terminal_validate_invariants(s);
        terminal_model_log("resize viewport=%dx%d grid=%dx%d", viewCols, viewRows, s->gridCols, s->gridRows);
    }
    int newScrollRows = term_grid_scrollback_count(&s->grid);
    unsigned long long newScrollCommits = term_grid_scrollback_commit_count(&s->grid);
    assert(oldScrollCommits == newScrollCommits);
    assert(oldScrollRows == newScrollRows);
    terminal_pipeline_log("resize old_vp=%dx%d new_vp=%dx%d old_grid=%dx%d new_grid=%dx%d old_cursor=%d,%d new_cursor=%d,%d scroll_rows=%d->%d commits=%llu->%llu",
                          oldViewportCols, oldViewportRows,
                          s->grid.viewport_cols, s->grid.viewport_rows,
                          oldGridCols, oldGridRows,
                          s->grid.cols, s->grid.rows,
                          oldCursorCol, oldCursorRow,
                          s->grid.cursor_col, s->grid.cursor_row,
                          oldScrollRows, newScrollRows,
                          oldScrollCommits, newScrollCommits);

    if (s->backend) {
        // Inform the shell only when terminal row/col dimensions changed.
        if (s->lastBackendRows != viewRows || s->lastBackendCols != viewCols) {
            terminal_backend_resize(s->backend, viewRows, viewCols);
            s->lastBackendRows = viewRows;
            s->lastBackendCols = viewCols;
        }
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
    s->inUse = true;
    s->isBuild = isBuild;
    s->isRun = isRun;
    s->id = g_next_id++;
    snprintf(s->name, sizeof(s->name), "%s", name ? name : (isBuild ? "Build" : (isRun ? "Run" : "Task")));
    term_grid_init(&s->grid, s->gridRows, s->gridCols);
    term_grid_set_scrollback_cap(&s->grid, terminal_scrollback_extra_rows());
    term_grid_set_alternate_screen_enabled(&s->grid, g_terminal_enable_alternate_screen ? 1 : 0);
    terminal_rebuild_session_model(s, "create_task");
    terminal_validate_invariants(s);
    return idx;
}
