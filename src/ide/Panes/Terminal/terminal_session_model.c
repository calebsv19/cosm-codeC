#include "ide/Panes/Terminal/terminal_internal.h"

#include "ide/Panes/Terminal/terminal_selection_helpers.h"

#include <assert.h>

static const PaneScrollConfig kTerminalScrollConfig = {
    .line_height_px = TERMINAL_LINE_HEIGHT,
    .deceleration_px = 0.0f,
    .allow_negative = false,
};

TerminalSession g_sessions[MAX_TERMINAL_SESSIONS];
int g_session_count = 0;
int g_active_index = 0;

TerminalSession* active_session(void) {
    if (g_session_count == 0) return NULL;
    if (g_active_index < 0 || g_active_index >= g_session_count) g_active_index = 0;
    return &g_sessions[g_active_index];
}

int terminal_scrollback_extra_rows(void) {
    if (g_terminal_scrollback_extra_rows > 0) return g_terminal_scrollback_extra_rows;
    return TERMINAL_SCROLLBACK_EXTRA_ROWS_DEFAULT;
}

static int terminal_clamp_rows(const TermGrid* grid, int rows) {
    if (!grid || grid->rows <= 0) return 1;
    if (rows < 1) rows = 1;
    if (rows > grid->rows) rows = grid->rows;
    return rows;
}

int terminal_session_content_rows(const TerminalSession* s) {
    if (!s) return 1;
    int viewportRows = s->grid.viewport_rows;
    if (viewportRows < 1 || viewportRows > s->grid.rows) viewportRows = s->grid.rows;
    if (s->grid.using_alternate) {
        return viewportRows > 0 ? viewportRows : 1;
    }
    int journalRows = terminal_journal_count(&s->journal);
    if (journalRows > 0) return journalRows;
    int rows = term_grid_scrollback_count(&s->grid) + viewportRows;
    return rows > 0 ? rows : 1;
}

int terminal_viewport_start_row(const TerminalSession* s) {
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

void terminal_rebuild_session_model(TerminalSession* s, const char* reason) {
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

    terminal_model_log("projection reason=%s mode=%s viewport=%dx%d cursor=%d,%d journal=%d scrollback=%d content=%d",
                       reason ? reason : "unknown",
                       visible.using_alternate ? "alternate" : "primary",
                       visible.rows,
                       visible.cols,
                       visible.cursor_row,
                       visible.cursor_col,
                       terminal_journal_count(&s->journal),
                       scrollback.row_count,
                       projectedRows);
    terminal_pipeline_log("snapshot reason=%s mode=%s cursor=%d,%d viewport=%dx%d projected=%d journal=%d scrollback=%d",
                          reason ? reason : "unknown",
                          visible.using_alternate ? "alternate" : "primary",
                          visible.cursor_row,
                          visible.cursor_col,
                          visible.rows,
                          visible.cols,
                          projectedRows,
                          terminal_journal_count(&s->journal),
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

void terminal_capture_journal_snapshot(TerminalSession* s, const char* reason) {
    if (!s || !s->grid.cells || s->grid.using_alternate) return;
    int viewportRows = s->grid.viewport_rows;
    if (viewportRows < 1 || viewportRows > s->grid.rows) viewportRows = s->grid.rows;
    int start = terminal_viewport_start_row(s);
    int durableBefore = s->journal.durable_count;
    float scrollOffsetBefore = s->scrollInitialized ? scroll_state_get_offset(&s->scrollState) : 0.0f;
    terminal_journal_capture_viewport(&s->journal, &s->grid, start, viewportRows, s->grid.cursor_row);
    int durableAfter = s->journal.durable_count;
    int durableAdded = durableAfter - durableBefore;
    if (durableAdded > 0 &&
        s->scrollInitialized &&
        !s->followOutput &&
        s->cellHeight > 0) {
        int firstVisibleRow = (int)(scrollOffsetBefore / (float)s->cellHeight);
        if (firstVisibleRow >= durableBefore) {
            float shift = (float)durableAdded * (float)s->cellHeight;
            s->scrollState.offset_px += shift;
            s->scrollState.target_offset_px += shift;
        }
    }
    terminal_pipeline_log("journal capture reason=%s rows=%d captures=%llu inserts=%llu drops=%llu",
                          reason ? reason : "unknown",
                          terminal_journal_count(&s->journal),
                          s->journal.capture_count,
                          s->journal.insert_count,
                          s->journal.drop_count);
}

void terminal_validate_invariants(const TerminalSession* s) {
    if (!s) return;
    const TermGrid* grid = &s->grid;
    if (!grid->cells || grid->rows <= 0 || grid->cols <= 0) return;
    assert(grid->cursor_row >= 0 && grid->cursor_row < grid->rows);
    assert(grid->cursor_col >= 0 && grid->cursor_col < grid->cols);

    int projectedRows = terminal_session_content_rows(s);
    int viewportRows = s->visibleModel.rows;
    int scrollbackRows = s->scrollbackModel.row_count;
    int journalRows = terminal_journal_count(&s->journal);
    (void)viewportRows;
    (void)scrollbackRows;
    (void)journalRows;
    if (!s->visibleModel.using_alternate) {
        if (journalRows > 0) {
            assert(projectedRows == journalRows);
        } else {
            assert(projectedRows == scrollbackRows + viewportRows);
        }
    } else {
        assert(scrollbackRows == 0);
    }

    terminal_selection_validate_projection_rows(projectedRows);
}

void terminal_ensure_session_scroll_state(TerminalSession* s) {
    if (!s) return;
    if (!s->scrollInitialized) {
        scroll_state_init(&s->scrollState, &kTerminalScrollConfig);
        s->scrollInitialized = true;
    }
    if (s->cellHeight > 0) {
        s->scrollState.line_height_px = (float)s->cellHeight;
    }
}

void ensure_terminal_scroll_state(void) {
    terminal_ensure_session_scroll_state(active_session());
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
    out_row->journal_row = -1;
    out_row->grid_row = -1;
    int journalRows = s->grid.using_alternate ? 0 : terminal_journal_count(&s->journal);
    if (journalRows > 0) {
        out_row->from_journal = true;
        out_row->journal_row = index;
        out_row->from_scrollback = false;
        return true;
    }
    out_row->from_journal = false;
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
    if (pr.from_journal) {
        const TermCell* rowCells = terminal_journal_row(&s->journal, pr.journal_row);
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

bool terminal_cursor_projection_position(int* out_row, int* out_col) {
    TerminalSession* s = active_session();
    if (!s || !s->grid.cells) return false;
    terminal_rebuild_session_model(s, "cursor_projection");

    int projectedRows = terminal_session_content_rows(s);
    if (projectedRows < 1) projectedRows = 1;

    int col = s->grid.cursor_col;
    if (col < 0) col = 0;
    if (col > s->grid.cols) col = s->grid.cols;

    int journalRows = s->grid.using_alternate ? 0 : terminal_journal_count(&s->journal);
    if (journalRows > 0) {
        const TermCell* cursorRow = term_grid_cell(&s->grid, s->grid.cursor_row, 0);
        int projectedRow = terminal_journal_find_last_equal_row(&s->journal, cursorRow, s->grid.cols);
        if (projectedRow < 0) projectedRow = journalRows - 1;
        if (projectedRow < 0 || projectedRow >= projectedRows) return false;
        if (out_row) *out_row = projectedRow;
        if (out_col) *out_col = col;
        return true;
    }

    int projectedRow = s->grid.cursor_row;
    if (!s->grid.using_alternate) {
        int viewportRows = s->grid.viewport_rows;
        if (viewportRows < 1 || viewportRows > s->grid.rows) viewportRows = s->grid.rows;
        int viewportStart = terminal_viewport_start_row(s);
        int localRow = s->grid.cursor_row - viewportStart;
        if (localRow < 0 || localRow >= viewportRows) return false;
        projectedRow = s->scrollbackModel.row_count + localRow;
    }

    if (projectedRow < 0 || projectedRow >= projectedRows) return false;
    if (out_row) *out_row = projectedRow;
    if (out_col) *out_col = col;
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
    out->journal_rows = terminal_journal_count(&s->journal);
    out->scrollback_rows = s->scrollbackModel.row_count;
    out->projected_rows = terminal_session_content_rows(s);
    out->cursor_visible = s->grid.cursor_visible != 0;
    out->follow_output = s->followOutput;
    out->scrollback_commits = term_grid_scrollback_commit_count(&s->grid);
    out->scrollback_drops = term_grid_scrollback_drop_count(&s->grid);
    out->journal_captures = s->journal.capture_count;
    out->journal_inserts = s->journal.insert_count;
    out->journal_drops = s->journal.drop_count;
    return true;
}
