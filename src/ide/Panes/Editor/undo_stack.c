// undo_stack.c


#include "ide/Panes/Editor/undo_stack.h"
#include "app/GlobalInfo/core_state.h"
#include "editor.h"
#include "ide/Panes/Editor/editor_view.h"
#include "ide/Panes/Editor/editor_buffer.h"
#include "ide/Panes/Editor/editor_state.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#define MAX_UNDO_ENTRIES 128
#define UNDO_STACK_MAGIC 0x554E444F53544143ULL

typedef struct {
    char* snapshot;
    size_t length;
    uint64_t contentRevision;
    int cursorRow;
    int cursorCol;
} UndoEntry;

typedef struct {
    uint64_t magic;
    UndoEntry undo[MAX_UNDO_ENTRIES];
    UndoEntry redo[MAX_UNDO_ENTRIES];
    int undoTop;
    int redoTop;

} UndoStack;



//              INITS
//      ==============================
//              STACK ACCESS





static bool undo_stack_is_valid(const UndoStack* stack) {
    if (!stack) return false;
    if (stack->magic != UNDO_STACK_MAGIC) return false;
    if (stack->undoTop < 0 || stack->undoTop > MAX_UNDO_ENTRIES) return false;
    if (stack->redoTop < 0 || stack->redoTop > MAX_UNDO_ENTRIES) return false;
    return true;
}

static UndoStack* getOrCreateUndoStack(OpenFile* file) {
    if (!file) return NULL;

    UndoStack* existing = (UndoStack*)file->undoStack;
    if (existing && !undo_stack_is_valid(existing)) {
        fprintf(stderr, "[Undo] Detected invalid undo stack pointer/state for '%s'; resetting history\n",
                file->filePath ? file->filePath : "(unknown)");
        file->undoStack = NULL;
        existing = NULL;
    }

    if (!existing) {
        UndoStack* newStack = malloc(sizeof(UndoStack));
        if (!newStack) return NULL;
        memset(newStack, 0, sizeof(UndoStack));
        newStack->magic = UNDO_STACK_MAGIC;
        file->undoStack = newStack;
        return newStack;
    }

    return existing;
}


static bool captureEntry(OpenFile* file, UndoEntry* entry) {
    entry->snapshot = getBufferSnapshot(file->buffer, &entry->length);
    entry->contentRevision = file->contentRevision;
    entry->cursorRow = file->state.cursorRow;
    entry->cursorCol = file->state.cursorCol;
    return entry->snapshot != NULL;
}

bool pushUndoState(OpenFile* file) {
    if (!file || !file->buffer) return false;
    UndoStack* stack = getOrCreateUndoStack(file);
    UndoEntry entry;
    if (!stack || !captureEntry(file, &entry)) return false;
    if (stack->undoTop == MAX_UNDO_ENTRIES) {
        free(stack->undo[0].snapshot);
        memmove(stack->undo, stack->undo + 1, (--stack->undoTop) * sizeof(entry));
    }
    stack->undo[stack->undoTop++] = entry;
    while (stack->redoTop) free(stack->redo[--stack->redoTop].snapshot);
    file->pendingTextEdit = false;
    return true;
}

static bool stepHistory(OpenFile* file, bool redo) {
    if (!file || !file->buffer || !file->undoStack) return false;
    UndoStack* stack = file->undoStack;
    if (!undo_stack_is_valid(stack)) return false;
    int* from = redo ? &stack->redoTop : &stack->undoTop;
    int* to = redo ? &stack->undoTop : &stack->redoTop;
    if (!*from || *to == MAX_UNDO_ENTRIES) return false;
    UndoEntry* entry = &(redo ? stack->redo : stack->undo)[*from - 1];
    UndoEntry current;
    if (!captureEntry(file, &current)) return false;
    /* Snapshot serialization appends a terminator for every row, including the last. */
    size_t length = entry->length ? entry->length - 1 : 0;
    if (!loadSnapshotIntoBuffer(file->buffer, entry->snapshot, length)) {
        free(current.snapshot);
        return false;
    }
    (redo ? stack->undo : stack->redo)[(*to)++] = current;
    --*from;
    free(entry->snapshot);
    file->state.cursorRow = entry->cursorRow;
    file->state.cursorCol = entry->cursorCol;
    file->state.selecting = false;
    file->pendingTextEdit = false;
    markFileAsModified(file);
    file->contentRevision = entry->contentRevision;
    file->isModified = file->saveFailed || file->contentRevision != file->savedContentRevision;
    return true;
}

bool performUndo(OpenFile* file) { return stepHistory(file, false); }
bool performRedo(OpenFile* file) { return stepHistory(file, true); }

void clearUndoHistory(OpenFile* file) {
    UndoStack* stack = file ? file->undoStack : NULL;
    if (!undo_stack_is_valid(stack)) return;
    while (stack->undoTop) free(stack->undo[--stack->undoTop].snapshot);
    while (stack->redoTop) free(stack->redo[--stack->redoTop].snapshot);
    file->pendingTextEdit = false;
}

bool beginWordEdit(OpenFile* file) {
    if (!file || (!file->pendingTextEdit && !pushUndoState(file))) return false;
    file->pendingTextEdit = true;
    return true;
}

void commitWordEdit(OpenFile* file) {
    if (file) file->pendingTextEdit = false;
}
