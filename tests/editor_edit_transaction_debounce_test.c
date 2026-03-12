#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "ide/Panes/Editor/editor_edit_transaction_core.h"

static int g_next_timer_id = 1;
static int g_last_scheduled_timer_id = -1;
static uint64_t g_last_scheduled_token = 0;
static uint32_t g_last_scheduled_delay_ms = 0;
static int g_cancel_count = 0;
static int g_last_canceled_timer_id = -1;

static int test_schedule_once(uint32_t delay_ms, uint64_t token, void* user_data) {
    (void)user_data;
    g_last_scheduled_delay_ms = delay_ms;
    g_last_scheduled_token = token;
    g_last_scheduled_timer_id = g_next_timer_id++;
    return g_last_scheduled_timer_id;
}

static void test_cancel_timer(int timer_id, void* user_data) {
    (void)user_data;
    g_cancel_count++;
    g_last_canceled_timer_id = timer_id;
}

static void reset_timer_stubs(void) {
    g_next_timer_id = 1;
    g_last_scheduled_timer_id = -1;
    g_last_scheduled_token = 0;
    g_last_scheduled_delay_ms = 0;
    g_cancel_count = 0;
    g_last_canceled_timer_id = -1;
}

static EditorEditTransactionCursor make_cursor(int row, int col, bool selecting, int sel_row, int sel_col) {
    EditorEditTransactionCursor cursor;
    cursor.cursor_row = row;
    cursor.cursor_col = col;
    cursor.selecting = selecting;
    cursor.sel_row = sel_row;
    cursor.sel_col = sel_col;
    return cursor;
}

static void test_debounce_commit_path(void) {
    reset_timer_stubs();
    EditorEditTransactionCoreState state;
    editor_edit_txn_core_init(&state, 300u);

    EditorEditTransactionCursor cursor = make_cursor(10, 4, false, 0, 0);
    EditorEditCommitReason reason = editor_edit_txn_core_note_edit(&state,
                                                                    (uintptr_t)0x1111,
                                                                    (uintptr_t)0xAAAA,
                                                                    2u,
                                                                    &cursor,
                                                                    test_schedule_once,
                                                                    test_cancel_timer,
                                                                    NULL,
                                                                    NULL);
    assert(reason == EDIT_COMMIT_REASON_NONE);
    assert(state.active);
    assert(state.starts == 1);
    assert(g_last_scheduled_delay_ms == 300u);
    assert(g_last_scheduled_token == state.generation);

    uintptr_t committed_file = 0;
    reason = editor_edit_txn_core_on_timer(&state, g_last_scheduled_token + 1u, &committed_file);
    assert(reason == EDIT_COMMIT_REASON_NONE);
    assert(state.active);

    reason = editor_edit_txn_core_on_timer(&state, g_last_scheduled_token, &committed_file);
    assert(reason == EDIT_COMMIT_REASON_DEBOUNCE);
    assert(committed_file == (uintptr_t)0x1111);
    assert(!state.active);
    assert(state.commits == 1);
    assert(state.debounce_commits == 1);
    assert(state.boundary_commits == 0);
}

static void test_boundary_commit_paths(void) {
    reset_timer_stubs();
    EditorEditTransactionCoreState state;
    editor_edit_txn_core_init(&state, 300u);

    EditorEditTransactionCursor cursor = make_cursor(3, 8, false, 0, 0);
    EditorEditCommitReason reason = editor_edit_txn_core_note_edit(&state,
                                                                    (uintptr_t)0x2222,
                                                                    (uintptr_t)0xBBBB,
                                                                    5u,
                                                                    &cursor,
                                                                    test_schedule_once,
                                                                    test_cancel_timer,
                                                                    NULL,
                                                                    NULL);
    assert(reason == EDIT_COMMIT_REASON_NONE);
    assert(state.active);
    assert(g_last_scheduled_timer_id > 0);

    uintptr_t committed_file = 0;
    EditorEditTransactionCursor moved_cursor = make_cursor(4, 0, false, 0, 0);
    reason = editor_edit_txn_core_update_context(&state,
                                                 (uintptr_t)0xBBBB,
                                                 (uintptr_t)0x2222,
                                                 &moved_cursor,
                                                 test_cancel_timer,
                                                 NULL,
                                                 &committed_file);
    assert(reason == EDIT_COMMIT_REASON_CURSOR_MOVE);
    assert(committed_file == (uintptr_t)0x2222);
    assert(!state.active);
    assert(state.boundary_commits == 1);
    assert(g_cancel_count == 1);
    assert(g_last_canceled_timer_id == g_last_scheduled_timer_id);

    cursor = make_cursor(1, 1, false, 0, 0);
    reason = editor_edit_txn_core_note_edit(&state,
                                            (uintptr_t)0x3333,
                                            (uintptr_t)0xCCCC,
                                            6u,
                                            &cursor,
                                            test_schedule_once,
                                            test_cancel_timer,
                                            NULL,
                                            NULL);
    assert(reason == EDIT_COMMIT_REASON_NONE);
    reason = editor_edit_txn_core_update_context(&state,
                                                 (uintptr_t)0xDDDD,
                                                 (uintptr_t)0x3333,
                                                 &cursor,
                                                 test_cancel_timer,
                                                 NULL,
                                                 &committed_file);
    assert(reason == EDIT_COMMIT_REASON_FOCUS_CHANGE);
    assert(!state.active);
}

static void test_file_switch_commit_on_new_edit(void) {
    reset_timer_stubs();
    EditorEditTransactionCoreState state;
    editor_edit_txn_core_init(&state, 300u);

    EditorEditTransactionCursor cursor = make_cursor(0, 0, false, 0, 0);
    EditorEditCommitReason reason = editor_edit_txn_core_note_edit(&state,
                                                                    (uintptr_t)0x4444,
                                                                    (uintptr_t)0xEEEE,
                                                                    1u,
                                                                    &cursor,
                                                                    test_schedule_once,
                                                                    test_cancel_timer,
                                                                    NULL,
                                                                    NULL);
    assert(reason == EDIT_COMMIT_REASON_NONE);
    assert(state.active);
    assert(state.file_token == (uintptr_t)0x4444);
    int first_timer_id = g_last_scheduled_timer_id;
    assert(first_timer_id > 0);
    uint64_t first_generation = state.generation;

    cursor = make_cursor(5, 2, false, 0, 0);
    uintptr_t committed_file = 0;
    reason = editor_edit_txn_core_note_edit(&state,
                                            (uintptr_t)0x5555,
                                            (uintptr_t)0xEEEE,
                                            2u,
                                            &cursor,
                                            test_schedule_once,
                                            test_cancel_timer,
                                            NULL,
                                            &committed_file);
    assert(reason == EDIT_COMMIT_REASON_FILE_SWITCH);
    assert(committed_file == (uintptr_t)0x4444);
    assert(state.active);
    assert(state.file_token == (uintptr_t)0x5555);
    assert(state.commits == 1);
    assert(state.boundary_commits == 1);
    assert(state.starts == 2);
    assert(state.generation == first_generation + 1u);
    assert(g_cancel_count == 1);
    assert(g_last_canceled_timer_id == first_timer_id);
}

int main(void) {
    test_debounce_commit_path();
    test_boundary_commit_paths();
    test_file_switch_commit_on_new_edit();
    puts("editor_edit_transaction_debounce_test: success");
    return 0;
}
