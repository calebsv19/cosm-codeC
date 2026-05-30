#include "ide/Panes/Terminal/terminal_internal.h"

#include "app/GlobalInfo/core_state.h"
#include "app/GlobalInfo/workspace_prefs.h"
#include "core/BuildSystem/build_diagnostics.h"
#include "core/Ipc/ide_ipc_server.h"
#include "engine/Render/render_font.h"
#include "engine/Render/render_text_helpers.h"

#include <SDL2/SDL_ttf.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

void terminal_jump_to_bottom(void) {
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

void printToTerminal(const char* text) {
    if (!text) return;
    size_t len = strlen(text);
    TerminalSession* s = active_session();
    if (!s) return;
    terminal_feed_bytes(s, text, len);
}

void clearTerminal(void) {
    terminal_clear_selection();
    TerminalSession* s = active_session();
    if (!s) return;
    ensure_terminal_scroll_state();
    s->scrollState.offset_px = 0.0f;
    s->scrollState.target_offset_px = 0.0f;
    term_grid_clear(&s->grid);
    terminal_journal_clear(&s->journal);
    s->lastViewportW = -1;
    s->lastViewportH = -1;
    s->lastBackendRows = -1;
    s->lastBackendCols = -1;
    terminal_rebuild_session_model(s, "clear");
    terminal_validate_invariants(s);
}

void terminal_notify_font_metrics_changed(void) {
    for (int i = 0; i < g_session_count; ++i) {
        g_sessions[i].lastViewportW = -1;
        g_sessions[i].lastViewportH = -1;
        if (g_sessions[i].scrollInitialized && g_sessions[i].cellHeight > 0) {
            g_sessions[i].scrollState.line_height_px = (float)g_sessions[i].cellHeight;
        }
    }
}

static bool terminal_flush_backend_output(TerminalSession* s) {
    bool changed = false;
    if (!s || !s->backend) return false;

    size_t len = 0;
    const char* data = terminal_backend_buffer(s->backend, &len);
    if (data && len > s->backendConsumed) {
        size_t chunkLen = len - s->backendConsumed;
        char* raw = (char*)malloc(chunkLen + 1);
        if (raw) {
            memcpy(raw, data + s->backendConsumed, chunkLen);
            raw[chunkLen] = '\0';
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
        const char* exitMsg = "[Terminal] Shell process exited.\n";
        terminal_feed_bytes(s, exitMsg, strlen(exitMsg));
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

    size_t len = strlen(path);
    size_t extra = 0;
    for (size_t i = 0; i < len; ++i) {
        if (path[i] == '\'') extra += 3;
    }
    size_t outCap = len + extra + 4;
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
    for (int i = 0; i < g_session_count; ++i) {
        TerminalSession* s = &g_sessions[i];
        if (!s->backend) continue;

        if (!s->backend->dead && terminal_backend_read_output(s->backend) > 0) {
            changed = true;
        }
        terminal_backend_poll_child(s->backend);
        if (terminal_flush_backend_output(s)) {
            changed = true;
        }
    }
    return changed;
}

void terminal_resize_grid_for_pane(int width_px, int height_px) {
    TerminalSession* s = active_session();
    if (!s) return;
    if (width_px == s->lastViewportW && height_px == s->lastViewportH) return;
    s->lastViewportW = width_px;
    s->lastViewportH = height_px;

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
    if (s->scrollInitialized) {
        s->scrollState.line_height_px = (float)s->cellHeight;
    }

    if (gridSizeChanged) {
        term_grid_resize(&s->grid, desiredRows, viewCols);
    }
    term_grid_set_scrollback_cap(&s->grid, terminal_scrollback_extra_rows());
    terminal_journal_configure(&s->journal, terminal_scrollback_extra_rows(), s->grid.cols);
    if (gridSizeChanged || viewportChanged) {
        term_grid_set_viewport_size(&s->grid, viewRows, viewCols);
        terminal_capture_journal_snapshot(s, "resize");
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
        if (s->lastBackendRows != viewRows || s->lastBackendCols != viewCols) {
            terminal_backend_resize(s->backend, viewRows, viewCols);
            s->lastBackendRows = viewRows;
            s->lastBackendCols = viewCols;
        }
    }
}
