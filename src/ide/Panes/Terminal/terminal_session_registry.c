#include "ide/Panes/Terminal/terminal_internal.h"

#include <string.h>

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

static void terminal_init_session(TerminalSession* s) {
    if (!s) return;
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
    terminal_init_session(s);
    snprintf(s->name, sizeof(s->name), "Term %d", termNumber);
    term_grid_init(&s->grid, s->gridRows, s->gridCols);
    term_grid_set_scrollback_cap(&s->grid, terminal_scrollback_extra_rows());
    term_grid_set_alternate_screen_enabled(&s->grid, g_terminal_enable_alternate_screen ? 1 : 0);
    terminal_journal_init(&s->journal, terminal_scrollback_extra_rows(), s->gridCols);
    terminal_rebuild_session_model(s, "create_interactive");
    terminal_validate_invariants(s);
    g_active_index = idx;
    terminal_spawn_shell(start_dir, 0, 0);
    return idx;
}

void terminal_close_interactive(int index) {
    if (index < 0 || index >= g_session_count) return;
    if (g_session_count <= 1) return;
    terminal_set_active(index);
    terminal_shutdown_shell();
    term_grid_free(&g_sessions[index].grid);
    terminal_journal_free(&g_sessions[index].journal);
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

bool terminal_can_close_active_interactive(void) {
    if (terminal_active_is_task()) return false;
    if (count_interactive() <= 1) return false;
    return true;
}

bool terminal_close_active_interactive(void) {
    if (!terminal_can_close_active_interactive()) return false;
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
    terminal_journal_clear(&s->journal);
    terminal_ensure_session_scroll_state(s);
    s->scrollState.offset_px = 0.0f;
    s->scrollState.target_offset_px = 0.0f;
    terminal_rebuild_session_model(s, "clear_session");
    terminal_validate_invariants(s);
}

int terminal_create_task_session(const char* name, bool isBuild, bool isRun) {
    if (g_session_count >= MAX_TERMINAL_SESSIONS) return -1;
    int idx = g_session_count++;
    TerminalSession* s = &g_sessions[idx];
    terminal_init_session(s);
    s->isBuild = isBuild;
    s->isRun = isRun;
    snprintf(s->name, sizeof(s->name), "%s", name ? name : (isBuild ? "Build" : (isRun ? "Run" : "Task")));
    term_grid_init(&s->grid, s->gridRows, s->gridCols);
    term_grid_set_scrollback_cap(&s->grid, terminal_scrollback_extra_rows());
    term_grid_set_alternate_screen_enabled(&s->grid, g_terminal_enable_alternate_screen ? 1 : 0);
    terminal_journal_init(&s->journal, terminal_scrollback_extra_rows(), s->gridCols);
    terminal_rebuild_session_model(s, "create_task");
    terminal_validate_invariants(s);
    return idx;
}
