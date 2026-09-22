#include "core/Ipc/ide_ipc_typing_edit.h"

#include "app/GlobalInfo/core_state.h"
#include "core/CommandBus/save_queue.h"
#include "core/Ipc/ide_ipc_edit_apply.h"
#include "core/Ipc/ide_ipc_path_guard.h"
#include "ide/Panes/Editor/editor_buffer.h"
#include "ide/Panes/Editor/editor_clipboard.h"
#include "ide/Panes/Editor/editor_state.h"
#include "ide/Panes/Editor/editor_text_edit.h"
#include "ide/Panes/Editor/editor_view.h"
#include "ide/Panes/Editor/undo_stack.h"

#include <SDL2/SDL.h>
#include <json-c/json.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    TYPING_UI_IDLE = 0,
    TYPING_UI_INSERTING,
    TYPING_UI_SAVING,
} TypingUiPhase;

typedef struct {
    TypingUiPhase phase;
    OpenFile* file;
    char absolute_path[1024];
    char relative_path[1024];
    size_t next_byte;
    size_t total_bytes;
    uint64_t next_due_ms;
    uint64_t before_revision;
    unsigned mutation_steps;
} TypingUiState;

static TypingUiState g_typing_ui;

static void set_error(char* out, size_t cap, const char* message) {
    if (out && cap) snprintf(out, cap, "%s", message ? message : "Visible typing failed");
}

static unsigned long long fnv1a64(const unsigned char* bytes, size_t length) {
    unsigned long long hash = 1469598103934665603ULL;
    for (size_t i = 0; i < length; ++i) {
        hash ^= (unsigned long long)bytes[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

static bool fnv_matches(const unsigned char* bytes, size_t length, const char* expected) {
    char actual[17];
    snprintf(actual, sizeof(actual), "%016llx", fnv1a64(bytes, length));
    return strcmp(actual, expected) == 0;
}

static char* read_file(const char* path, size_t* length_out) {
    if (length_out) *length_out = 0;
    FILE* file = fopen(path, "rb");
    if (!file) return NULL;
    if (fseek(file, 0, SEEK_END) != 0) { fclose(file); return NULL; }
    long length = ftell(file);
    if (length < 0 || length > 64 * 1024 * 1024 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }
    char* bytes = malloc((size_t)length + 1u);
    if (!bytes) { fclose(file); return NULL; }
    size_t read_length = fread(bytes, 1, (size_t)length, file);
    fclose(file);
    if (read_length != (size_t)length) { free(bytes); return NULL; }
    bytes[read_length] = '\0';
    if (length_out) *length_out = read_length;
    return bytes;
}

static void find_open_document(EditorView* view,
                               const char* path,
                               EditorView** found_view,
                               int* found_tab,
                               int* found_count,
                               bool* unsaved) {
    if (!view) return;
    if (view->type == VIEW_SPLIT) {
        find_open_document(view->childA, path, found_view, found_tab, found_count, unsaved);
        find_open_document(view->childB, path, found_view, found_tab, found_count, unsaved);
        return;
    }
    for (int i = 0; i < view->fileCount; ++i) {
        OpenFile* file = view->openFiles[i];
        if (file && file->filePath && strcmp(file->filePath, path) == 0) {
            (*found_count)++;
            if (file->isModified) *unsaved = true;
            if (!*found_view) {
                *found_view = view;
                *found_tab = i;
            }
        }
    }
}

static bool position_to_offset(const EditorBuffer* buffer,
                               int line,
                               int column,
                               size_t* offset_out) {
    if (!buffer || !offset_out || line < 1 || column < 1 || line > buffer->lineCount) return false;
    int row = line - 1;
    size_t column_zero = (size_t)(column - 1);
    size_t line_length = strlen(buffer->lines[row] ? buffer->lines[row] : "");
    if (column_zero > line_length) return false;
    size_t offset = 0;
    for (int i = 0; i < row; ++i) {
        offset += strlen(buffer->lines[i] ? buffer->lines[i] : "") + 1u;
    }
    *offset_out = offset + column_zero;
    return true;
}

static IdeIpcTypingStepResult begin_typing(const IdeIpcTypingRequest* request,
                                            const char* project_root,
                                            char* error_out,
                                            size_t error_out_cap) {
    memset(&g_typing_ui, 0, sizeof(g_typing_ui));
    if (!project_root || !project_root[0] || isSaveQueueBusy()) {
        set_error(error_out, error_out_cap, "Visible typing requires a workspace and idle save queue");
        return IDE_IPC_TYPING_STEP_FAILED;
    }
    if (!ide_ipc_resolve_workspace_existing_path(project_root, request->path,
                                                  g_typing_ui.absolute_path,
                                                  sizeof(g_typing_ui.absolute_path),
                                                  error_out, error_out_cap)) {
        return IDE_IPC_TYPING_STEP_FAILED;
    }
    snprintf(g_typing_ui.relative_path, sizeof(g_typing_ui.relative_path), "%s", request->path);

    char disk_hash[17] = {0};
    if (!ide_ipc_compute_file_hash_hex(g_typing_ui.absolute_path, disk_hash, sizeof(disk_hash)) ||
        strcmp(disk_hash, request->expected_before_fnv64) != 0) {
        set_error(error_out, error_out_cap, "Saved file changed since the typing request was reviewed");
        return IDE_IPC_TYPING_STEP_FAILED;
    }

    IDECoreState* core = getCoreState();
    if (!core) {
        set_error(error_out, error_out_cap, "IDE editor state unavailable");
        return IDE_IPC_TYPING_STEP_FAILED;
    }
    EditorView* view = NULL;
    int tab = -1;
    int open_count = 0;
    bool unsaved = false;
    find_open_document(core->persistentEditorView, g_typing_ui.absolute_path,
                       &view, &tab, &open_count, &unsaved);
    if (unsaved || open_count > 1) {
        set_error(error_out, error_out_cap,
                  unsaved ? "Open document has unsaved edits" :
                            "Document is open in multiple editor panes");
        return IDE_IPC_TYPING_STEP_FAILED;
    }
    if (!view) {
        view = core->activeEditorView ? core->activeEditorView : core->persistentEditorView;
        if (view && view->type != VIEW_LEAF) view = findNextLeaf(view);
        if (!view) {
            set_error(error_out, error_out_cap, "No editor view available for visible typing");
            return IDE_IPC_TYPING_STEP_FAILED;
        }
        g_typing_ui.file = openFileInView(view, g_typing_ui.absolute_path);
        tab = view->activeTab;
    } else {
        g_typing_ui.file = view->openFiles[tab];
        view->activeTab = tab;
    }
    if (!g_typing_ui.file || !g_typing_ui.file->buffer || g_typing_ui.file->isModified) {
        set_error(error_out, error_out_cap, "Failed to open a clean editor buffer for visible typing");
        return IDE_IPC_TYPING_STEP_FAILED;
    }
    setActiveEditorView(view);
    core->activeEditorView = view;
    if (core->editorPane) core->focusedPane = core->editorPane;

    size_t snapshot_length = 0;
    char* snapshot = getBufferSnapshot(g_typing_ui.file->buffer, &snapshot_length);
    size_t start_offset = 0;
    size_t end_offset = 0;
    if (!snapshot || !fnv_matches((const unsigned char*)snapshot, snapshot_length,
                                  request->expected_before_fnv64) ||
        !position_to_offset(g_typing_ui.file->buffer, request->start_line,
                            request->start_column, &start_offset) ||
        !position_to_offset(g_typing_ui.file->buffer, request->end_line,
                            request->end_column, &end_offset) ||
        end_offset < start_offset || end_offset > snapshot_length ||
        strlen(request->expected_text) != end_offset - start_offset ||
        memcmp(snapshot + start_offset, request->expected_text,
               end_offset - start_offset) != 0) {
        free(snapshot);
        set_error(error_out, error_out_cap,
                  "Editor buffer or selected text changed before visible typing began");
        return IDE_IPC_TYPING_STEP_FAILED;
    }
    free(snapshot);

    EditorState* state = &g_typing_ui.file->state;
    state->selStartRow = request->start_line - 1;
    state->selStartCol = request->start_column - 1;
    state->cursorRow = request->end_line - 1;
    state->cursorCol = request->end_column - 1;
    state->selecting = end_offset > start_offset;
    g_typing_ui.before_revision = g_typing_ui.file->documentRevision;
    if (state->selecting) {
        pushUndoState(g_typing_ui.file);
        if (!removeSelectedText(g_typing_ui.file->buffer, state)) {
            set_error(error_out, error_out_cap, "Failed to remove the checked typing range");
            return IDE_IPC_TYPING_STEP_FAILED;
        }
        markFileAsModified(g_typing_ui.file);
        g_typing_ui.mutation_steps++;
    }
    editorStateFrameLineInUpperBand(state, state->cursorRow,
                                    view->h > 0 ? view->h : 600,
                                    g_typing_ui.file->buffer->lineCount);
    g_typing_ui.total_bytes = strlen(request->replacement_text);
    g_typing_ui.next_due_ms = SDL_GetTicks64() + (uint64_t)request->cadence_ms;
    g_typing_ui.phase = TYPING_UI_INSERTING;
    return IDE_IPC_TYPING_STEP_CONTINUE;
}

static IdeIpcTypingStepResult finish_typing(const IdeIpcTypingRequest* request,
                                             char* error_out,
                                             size_t error_out_cap,
                                             json_object** result_out) {
    if (isSaveQueueBusy()) return IDE_IPC_TYPING_STEP_CONTINUE;
    size_t saved_length = 0;
    char* saved = read_file(g_typing_ui.absolute_path, &saved_length);
    if (!saved || !fnv_matches((const unsigned char*)saved, saved_length,
                               request->expected_after_fnv64)) {
        free(saved);
        set_error(error_out, error_out_cap,
                  "Visible typing is partial or save readback does not match the expected result");
        g_typing_ui.phase = TYPING_UI_IDLE;
        return IDE_IPC_TYPING_STEP_FAILED;
    }
    free(saved);

    json_object* result = json_object_new_object();
    json_object_object_add(result, "typed", json_object_new_boolean(true));
    json_object_object_add(result, "path", json_object_new_string(g_typing_ui.relative_path));
    json_object_object_add(result, "typed_bytes", json_object_new_int64((int64_t)g_typing_ui.total_bytes));
    json_object_object_add(result, "cadence_ms", json_object_new_int(request->cadence_ms));
    json_object_object_add(result, "mutation_steps", json_object_new_int64(g_typing_ui.mutation_steps));
    json_object_object_add(result, "document_revision_before",
                           json_object_new_int64((int64_t)g_typing_ui.before_revision));
    json_object_object_add(result, "document_revision_after",
                           json_object_new_int64((int64_t)g_typing_ui.file->documentRevision));
    json_object_object_add(result, "saved_readback", json_object_new_boolean(true));
    json_object_object_add(result, "before_fnv64",
                           json_object_new_string(request->expected_before_fnv64));
    json_object_object_add(result, "after_fnv64",
                           json_object_new_string(request->expected_after_fnv64));
    json_object_object_add(result, "intermediate_frames_captured",
                           json_object_new_boolean(false));
    if (result_out) *result_out = result; else json_object_put(result);
    g_typing_ui.phase = TYPING_UI_IDLE;
    return IDE_IPC_TYPING_STEP_COMPLETE;
}

IdeIpcTypingStepResult ide_ipc_typing_edit_step(
    const IdeIpcTypingRequest* request,
    bool begin,
    char* error_out,
    size_t error_out_cap,
    void* userdata,
    json_object** result_out) {
    const char* project_root = (const char*)userdata;
    if (result_out) *result_out = NULL;
    if (begin) return begin_typing(request, project_root, error_out, error_out_cap);
    if (!request || !g_typing_ui.file || g_typing_ui.phase == TYPING_UI_IDLE) {
        set_error(error_out, error_out_cap, "Visible typing state was lost");
        return IDE_IPC_TYPING_STEP_FAILED;
    }
    if (g_typing_ui.phase == TYPING_UI_SAVING) {
        return finish_typing(request, error_out, error_out_cap, result_out);
    }

    uint64_t now = SDL_GetTicks64();
    if (now < g_typing_ui.next_due_ms) return IDE_IPC_TYPING_STEP_CONTINUE;
    if (g_typing_ui.next_byte >= g_typing_ui.total_bytes) {
        set_error(error_out, error_out_cap, "Visible typing entered an invalid insertion state");
        return IDE_IPC_TYPING_STEP_FAILED;
    }

    OpenFile* file = g_typing_ui.file;
    uint64_t revision_before = file->documentRevision;
    char byte = request->replacement_text[g_typing_ui.next_byte];
    if (byte == '\n') {
        handleCommandInsertNewline(file, file->buffer, &file->state);
    } else {
        handleCommandInsertCharacter(file, file->buffer, &file->state, byte);
    }
    if (file->documentRevision == revision_before) {
        set_error(error_out, error_out_cap,
                  "Editor rejected a visible typing character; the document is partial and unsaved");
        return IDE_IPC_TYPING_STEP_FAILED;
    }
    g_typing_ui.next_byte++;
    g_typing_ui.mutation_steps++;
    g_typing_ui.next_due_ms = now + (uint64_t)request->cadence_ms;
    IDECoreState* core = getCoreState();
    EditorView* view = core ? core->activeEditorView : NULL;
    editorStateFrameLineInUpperBand(&file->state, file->state.cursorRow,
                                    view && view->h > 0 ? view->h : 600,
                                    file->buffer->lineCount);

    if (g_typing_ui.next_byte < g_typing_ui.total_bytes) {
        return IDE_IPC_TYPING_STEP_CONTINUE;
    }

    commitWordEdit(file);
    size_t final_length = 0;
    char* final_snapshot = getBufferSnapshot(file->buffer, &final_length);
    if (!final_snapshot || !fnv_matches((const unsigned char*)final_snapshot, final_length,
                                        request->expected_after_fnv64)) {
        free(final_snapshot);
        set_error(error_out, error_out_cap,
                  "Visible typing completed in memory but final bytes differ; document remains unsaved");
        return IDE_IPC_TYPING_STEP_FAILED;
    }
    free(final_snapshot);
    queueSaveFromOpenFile(file);
    g_typing_ui.phase = TYPING_UI_SAVING;
    return IDE_IPC_TYPING_STEP_CONTINUE;
}
