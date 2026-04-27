#include "ide/Panes/Editor/editor_view_file_helpers.h"

#include <stdio.h>
#include <string.h>

#include "core/LoopEvents/event_queue.h"

const char* getFileName(const char* path) {
    const char* slash = strrchr(path, '/');
    return (slash) ? slash + 1 : path;
}

void markFileAsModified(OpenFile* file) {
    if (!file) return;
    file->isModified = true;
    file->bufferVersion++;
    file->documentRevision++;
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

    freeEditorBuffer(file->buffer);
    file->buffer = newBuffer;
    file->bufferVersion++;
    file->documentRevision++;
    loop_events_emit_document_revision_changed(file->filePath, file->documentRevision);
    editor_invalidate_file_projection(file);

    file->isModified = false;
    file->showSavedTag = false;

    printf("[Watcher] Reloaded: %s\n", file->filePath);
}
