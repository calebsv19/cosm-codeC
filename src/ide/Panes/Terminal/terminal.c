#include "terminal.h"
#include "ide/Panes/Terminal/terminal_internal.h"
#include "app/GlobalInfo/core_state.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

static bool g_terminal_safe_paste_enabled = true;

enum {
    TERMINAL_SCROLLBACK_EXTRA_ROWS_MIN = 2000,
    TERMINAL_SCROLLBACK_EXTRA_ROWS_MAX = 200000,
};
int g_terminal_scrollback_extra_rows = TERMINAL_SCROLLBACK_EXTRA_ROWS_DEFAULT;
static bool g_terminal_debug_model_enabled = false;
static bool g_terminal_debug_overlay_enabled = false;
static bool g_terminal_debug_pipeline_enabled = false;
bool g_terminal_enable_alternate_screen = true;
static unsigned long long g_terminal_pipeline_bytes = 0ull;
static unsigned long long g_terminal_pipeline_emulator_feed_calls = 0ull;
static unsigned long long g_terminal_pipeline_legacy_feed_calls = 0ull;

void terminal_model_log(const char* fmt, ...) {
    if (!g_terminal_debug_model_enabled || !fmt) return;
    va_list args;
    va_start(args, fmt);
    printf("[TerminalModel] ");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
}

void terminal_pipeline_log(const char* fmt, ...) {
    if (!g_terminal_debug_pipeline_enabled || !fmt) return;
    va_list args;
    va_start(args, fmt);
    printf("[TerminalPipeline] ");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
}

void terminal_feed_bytes(TerminalSession* s, const char* bytes, size_t len) {
    if (!s || !bytes || len == 0) return;
    g_terminal_pipeline_bytes += (unsigned long long)len;
    g_terminal_pipeline_emulator_feed_calls++;
    unsigned long long commits_before = term_grid_scrollback_commit_count(&s->grid);
    bool wasAlternate = s->grid.using_alternate != 0;
    term_emulator_feed(&s->grid, bytes, len);
    bool nowAlternate = s->grid.using_alternate != 0;
    unsigned long long commits_after = term_grid_scrollback_commit_count(&s->grid);
    (void)commits_before;
    (void)commits_after;
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
    terminal_capture_journal_snapshot(s, "feed");
    terminal_rebuild_session_model(s, "feed");
    terminal_validate_invariants(s);
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

void initTerminal() {
    terminal_clear_selection();
    const char* envRows = getenv("IDE_TERMINAL_SCROLLBACK_ROWS");
    if (envRows && envRows[0]) {
        int parsed = atoi(envRows);
        if (parsed >= TERMINAL_SCROLLBACK_EXTRA_ROWS_MIN &&
            parsed <= TERMINAL_SCROLLBACK_EXTRA_ROWS_MAX) {
            g_terminal_scrollback_extra_rows = parsed;
        } else {
            g_terminal_scrollback_extra_rows = TERMINAL_SCROLLBACK_EXTRA_ROWS_DEFAULT;
        }
    } else {
        g_terminal_scrollback_extra_rows = TERMINAL_SCROLLBACK_EXTRA_ROWS_DEFAULT;
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
    g_terminal_enable_alternate_screen =
        (altScreen && altScreen[0] && strcmp(altScreen, "0") != 0);
    g_session_count = 0;
    g_active_index = 0;
    int buildIdx = terminal_create_task_session("Build", true, false);
    int runIdx = terminal_create_task_session("Run", false, true);
    (void)buildIdx; (void)runIdx;
    int interIdx = terminal_create_interactive(getWorkspacePath());
    if (interIdx >= 0) terminal_set_active(interIdx);
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
