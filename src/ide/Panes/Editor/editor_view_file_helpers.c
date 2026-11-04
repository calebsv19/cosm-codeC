#include "ide/Panes/Editor/editor_view_file_helpers.h"

#include "undo_stack.h"
#include <stdio.h>
#include <string.h>

#include "core/LoopEvents/event_queue.h"

const char* getFileName(const char* path) {
    const char* slash = strrchr(path, '/');
    return (slash) ? slash + 1 : path;
}

void markFileAsModified(OpenFile* file) {
    if (!file) return;
    file->showSavedTag = false;
    file->isModified = true;
    file->bufferVersion++;
    file->contentRevision = ++file->documentRevision;
    loop_events_emit_document_edited(file->filePath, file->documentRevision);
    editor_invalidate_file_projection(file);
    editor_edit_transaction_note_document_edit(file);
}

uint64_t open_file_document_revision(const OpenFile* file) {
    return file ? file->documentRevision : 0u;
}

void reloadOpenFileFromDisk(OpenFile* file) {
    if (!file || !file->filePath) return;

    EditorBuffer* newBuffer = loadEditorBuffer(file->filePath);
    if (!newBuffer) {
        printf("[Watcher] Failed to reload: %s\n", file->filePath);
        return;
    }

    /* A watcher can observe the editor's own save. Preserve its undo chain
       when the new buffer represents exactly the same rows. */
    bool same = file->buffer && file->buffer->lineCount == newBuffer->lineCount;
    for (int i = 0; same && i < newBuffer->lineCount; ++i)
        same = strcmp(file->buffer->lines[i], newBuffer->lines[i]) == 0;
    if (!same) clearUndoHistory(file);
    file->pendingTextEdit = false;
    file->commandTargetId = 0;
    freeEditorBuffer(file->buffer);
    file->buffer = newBuffer;
    file->bufferVersion++;
    ++file->documentRevision;
    if (!same) file->contentRevision = file->documentRevision;
    loop_events_emit_document_revision_changed(file->filePath, file->documentRevision);
    editor_invalidate_file_projection(file);

    file->savedContentRevision = file->contentRevision;
    file->saveFailed = false;
    file->isModified = false;
    file->showSavedTag = false;

    printf("[Watcher] Reloaded: %s\n", file->filePath);
}
