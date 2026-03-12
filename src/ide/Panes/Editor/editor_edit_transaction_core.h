#ifndef EDITOR_EDIT_TRANSACTION_CORE_H
#define EDITOR_EDIT_TRANSACTION_CORE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum EditorEditCommitReason {
    EDIT_COMMIT_REASON_NONE = 0,
    EDIT_COMMIT_REASON_DEBOUNCE = 1,
    EDIT_COMMIT_REASON_CURSOR_MOVE = 2,
    EDIT_COMMIT_REASON_SELECTION_CHANGE = 3,
    EDIT_COMMIT_REASON_FOCUS_CHANGE = 4,
    EDIT_COMMIT_REASON_FILE_SWITCH = 5
} EditorEditCommitReason;

typedef struct EditorEditTransactionCursor {
    int cursor_row;
    int cursor_col;
    bool selecting;
    int sel_row;
    int sel_col;
} EditorEditTransactionCursor;

typedef struct EditorEditTransactionSnapshot {
    bool active;
    uint32_t debounce_ms;
    uint64_t generation;
    uint64_t starts;
    uint64_t commits;
    uint64_t debounce_commits;
    uint64_t boundary_commits;
    uint64_t active_revision;
    EditorEditCommitReason last_reason;
} EditorEditTransactionSnapshot;

typedef int (*EditorEditTxnScheduleOnceFn)(uint32_t delay_ms, uint64_t token, void* user_data);
typedef void (*EditorEditTxnCancelFn)(int timer_id, void* user_data);

typedef struct EditorEditTransactionCoreState {
    bool active;
    uint32_t debounce_ms;
    uint64_t generation;
    int debounce_timer_id;
    uintptr_t file_token;
    uintptr_t view_token;
    EditorEditTransactionCursor last_cursor;
    uint64_t active_revision;
    uint64_t starts;
    uint64_t commits;
    uint64_t debounce_commits;
    uint64_t boundary_commits;
    EditorEditCommitReason last_reason;
} EditorEditTransactionCoreState;

void editor_edit_txn_core_init(EditorEditTransactionCoreState* state, uint32_t debounce_ms);
void editor_edit_txn_core_reset(EditorEditTransactionCoreState* state,
                                EditorEditTxnCancelFn cancel_fn,
                                void* user_data);
void editor_edit_txn_core_snapshot(const EditorEditTransactionCoreState* state,
                                   EditorEditTransactionSnapshot* out);
EditorEditCommitReason editor_edit_txn_core_note_edit(EditorEditTransactionCoreState* state,
                                                      uintptr_t file_token,
                                                      uintptr_t view_token,
                                                      uint64_t revision,
                                                      const EditorEditTransactionCursor* cursor,
                                                      EditorEditTxnScheduleOnceFn schedule_fn,
                                                      EditorEditTxnCancelFn cancel_fn,
                                                      void* user_data,
                                                      uintptr_t* out_committed_file_token);
EditorEditCommitReason editor_edit_txn_core_on_timer(EditorEditTransactionCoreState* state,
                                                     uint64_t timer_token,
                                                     uintptr_t* out_committed_file_token);
EditorEditCommitReason editor_edit_txn_core_update_context(EditorEditTransactionCoreState* state,
                                                           uintptr_t view_token,
                                                           uintptr_t file_token,
                                                           const EditorEditTransactionCursor* cursor,
                                                           EditorEditTxnCancelFn cancel_fn,
                                                           void* user_data,
                                                           uintptr_t* out_committed_file_token);

#endif // EDITOR_EDIT_TRANSACTION_CORE_H
