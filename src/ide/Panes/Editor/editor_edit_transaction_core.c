#include "ide/Panes/Editor/editor_edit_transaction_core.h"

#include <string.h>

static EditorEditCommitReason commit_core(EditorEditTransactionCoreState* state,
                                          EditorEditCommitReason reason,
                                          EditorEditTxnCancelFn cancel_fn,
                                          void* user_data,
                                          uintptr_t* out_committed_file_token) {
    if (!state || !state->active) return EDIT_COMMIT_REASON_NONE;

    if (out_committed_file_token) *out_committed_file_token = state->file_token;
    if (state->debounce_timer_id > 0 && cancel_fn) {
        cancel_fn(state->debounce_timer_id, user_data);
    }
    state->debounce_timer_id = -1;
    state->active = false;
    state->file_token = 0;
    state->view_token = 0;
    state->commits++;
    if (reason == EDIT_COMMIT_REASON_DEBOUNCE) {
        state->debounce_commits++;
    } else {
        state->boundary_commits++;
    }
    state->last_reason = reason;
    return reason;
}

void editor_edit_txn_core_init(EditorEditTransactionCoreState* state, uint32_t debounce_ms) {
    if (!state) return;
    memset(state, 0, sizeof(*state));
    state->debounce_ms = debounce_ms;
    state->debounce_timer_id = -1;
    state->last_reason = EDIT_COMMIT_REASON_NONE;
}

void editor_edit_txn_core_reset(EditorEditTransactionCoreState* state,
                                EditorEditTxnCancelFn cancel_fn,
                                void* user_data) {
    if (!state) return;
    if (state->debounce_timer_id > 0 && cancel_fn) {
        cancel_fn(state->debounce_timer_id, user_data);
    }
    uint32_t debounce_ms = state->debounce_ms ? state->debounce_ms : 300u;
    memset(state, 0, sizeof(*state));
    state->debounce_ms = debounce_ms;
    state->debounce_timer_id = -1;
    state->last_reason = EDIT_COMMIT_REASON_NONE;
}

void editor_edit_txn_core_snapshot(const EditorEditTransactionCoreState* state,
                                   EditorEditTransactionSnapshot* out) {
    if (!state || !out) return;
    out->active = state->active;
    out->debounce_ms = state->debounce_ms;
    out->generation = state->generation;
    out->starts = state->starts;
    out->commits = state->commits;
    out->debounce_commits = state->debounce_commits;
    out->boundary_commits = state->boundary_commits;
    out->active_revision = state->active_revision;
    out->last_reason = state->last_reason;
}

EditorEditCommitReason editor_edit_txn_core_note_edit(EditorEditTransactionCoreState* state,
                                                      uintptr_t file_token,
                                                      uintptr_t view_token,
                                                      uint64_t revision,
                                                      const EditorEditTransactionCursor* cursor,
                                                      EditorEditTxnScheduleOnceFn schedule_fn,
                                                      EditorEditTxnCancelFn cancel_fn,
                                                      void* user_data,
                                                      uintptr_t* out_committed_file_token) {
    if (!state || !file_token) return EDIT_COMMIT_REASON_NONE;
    EditorEditCommitReason commit_reason = EDIT_COMMIT_REASON_NONE;
    if (state->active && state->file_token != file_token) {
        commit_reason = commit_core(state,
                                    EDIT_COMMIT_REASON_FILE_SWITCH,
                                    cancel_fn,
                                    user_data,
                                    out_committed_file_token);
    }

    if (!state->active || state->file_token != file_token) {
        state->active = true;
        state->generation++;
        state->file_token = file_token;
        state->view_token = view_token;
        state->starts++;
    }

    state->active_revision = revision;
    if (cursor) state->last_cursor = *cursor;

    if (state->debounce_timer_id > 0 && cancel_fn) {
        cancel_fn(state->debounce_timer_id, user_data);
    }
    state->debounce_timer_id = -1;
    if (schedule_fn) {
        state->debounce_timer_id = schedule_fn(state->debounce_ms, state->generation, user_data);
    }
    return commit_reason;
}

EditorEditCommitReason editor_edit_txn_core_on_timer(EditorEditTransactionCoreState* state,
                                                     uint64_t timer_token,
                                                     uintptr_t* out_committed_file_token) {
    if (!state || !state->active) return EDIT_COMMIT_REASON_NONE;
    if (timer_token != state->generation) return EDIT_COMMIT_REASON_NONE;
    state->debounce_timer_id = -1;
    return commit_core(state,
                       EDIT_COMMIT_REASON_DEBOUNCE,
                       NULL,
                       NULL,
                       out_committed_file_token);
}

EditorEditCommitReason editor_edit_txn_core_update_context(EditorEditTransactionCoreState* state,
                                                           uintptr_t view_token,
                                                           uintptr_t file_token,
                                                           const EditorEditTransactionCursor* cursor,
                                                           EditorEditTxnCancelFn cancel_fn,
                                                           void* user_data,
                                                           uintptr_t* out_committed_file_token) {
    if (!state || !state->active) return EDIT_COMMIT_REASON_NONE;

    if (view_token != state->view_token) {
        return commit_core(state,
                           EDIT_COMMIT_REASON_FOCUS_CHANGE,
                           cancel_fn,
                           user_data,
                           out_committed_file_token);
    }
    if (file_token != state->file_token) {
        return commit_core(state,
                           EDIT_COMMIT_REASON_FILE_SWITCH,
                           cancel_fn,
                           user_data,
                           out_committed_file_token);
    }
    if (!cursor) return EDIT_COMMIT_REASON_NONE;

    if (cursor->cursor_row != state->last_cursor.cursor_row ||
        cursor->cursor_col != state->last_cursor.cursor_col) {
        return commit_core(state,
                           EDIT_COMMIT_REASON_CURSOR_MOVE,
                           cancel_fn,
                           user_data,
                           out_committed_file_token);
    }

    if (cursor->selecting != state->last_cursor.selecting ||
        cursor->sel_row != state->last_cursor.sel_row ||
        cursor->sel_col != state->last_cursor.sel_col) {
        return commit_core(state,
                           EDIT_COMMIT_REASON_SELECTION_CHANGE,
                           cancel_fn,
                           user_data,
                           out_committed_file_token);
    }

    return EDIT_COMMIT_REASON_NONE;
}
