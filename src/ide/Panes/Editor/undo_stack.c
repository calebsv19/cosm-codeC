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
    int cursorRow;
    int cursorCol;
} UndoEntry;

typedef struct {
    uint64_t magic;
    UndoEntry undo[MAX_UNDO_ENTRIES];
    UndoEntry redo[MAX_UNDO_ENTRIES];
    int undoTop;
    int redoTop;

    // === Live edit session ===
    bool pendingTextEdit;
    int pendingStartRow;
    int pendingStartCol;
    char* typedWord;
    size_t typedWordLen;
    size_t typedWordCap;
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


void pushUndoState(OpenFile* file){
    if (!file || !file->buffer) return;

    UndoStack* stack = getOrCreateUndoStack(file);
    if (!stack) return;

    if (stack->undoTop >= MAX_UNDO_ENTRIES) {
        // Drop oldest
        free(stack->undo[0].snapshot);
        memmove(&stack->undo[0], &stack->undo[1], sizeof(UndoEntry) * (MAX_UNDO_ENTRIES - 1));
        stack->undoTop = MAX_UNDO_ENTRIES - 1;
    }

    size_t len = 0;
    char* snapshot = getBufferSnapshot(file->buffer, &len);
    if (!snapshot || len == 0) return;

    stack->undo[stack->undoTop].snapshot = snapshot;
    stack->undo[stack->undoTop].length = len;
    stack->undo[stack->undoTop].cursorRow = file->state.cursorRow;
    stack->undo[stack->undoTop].cursorCol = file->state.cursorCol;
    stack->undoTop++;

    // Clear redo stack on new action
    for (int i = 0; i < stack->redoTop; i++) {
        free(stack->redo[i].snapshot);
    }
    stack->redoTop = 0;
}


bool performUndo(OpenFile* file) {
    if (!file || !file->buffer) return false;
    UndoStack* stack = getOrCreateUndoStack(file);
    if (stack->undoTop == 0) return false;

    // Move current state to redo stack
    size_t len = 0;
    char* snapshot = getBufferSnapshot(file->buffer, &len);
    if (snapshot && len > 0 && stack->redoTop < MAX_UNDO_ENTRIES) {
        stack->redo[stack->redoTop].snapshot = snapshot;
        stack->redo[stack->redoTop].length = len;
        stack->redo[stack->redoTop].cursorRow = file->state.cursorRow;
        stack->redo[stack->redoTop].cursorCol = file->state.cursorCol;
        stack->redoTop++;
    }

    // Pop from undo
    stack->undoTop--;
    UndoEntry* entry = &stack->undo[stack->undoTop];

    IDECoreState* core = getCoreState();
    EditorView* activeEditorView = core->activeEditorView;

    loadSnapshotIntoBuffer(file->buffer, entry->snapshot, entry->length);
    if (activeEditorView && activeEditorView->activeTab >= 0 &&
       	 	activeEditorView->openFiles[activeEditorView->activeTab] == file) {
     
        editorBuffer = file->buffer;
    }



    file->state.cursorRow = entry->cursorRow;
    file->state.cursorCol = entry->cursorCol;

    file->state.selecting = false;

    stack->pendingTextEdit = false;
    free(stack->typedWord);
    stack->typedWord = NULL;
    stack->typedWordLen = 0;
    stack->typedWordCap = 0;


    return true;
}


bool performRedo(OpenFile* file) {
    if (!file || !file->buffer) return false;
    UndoStack* stack = getOrCreateUndoStack(file);
    if (stack->redoTop == 0) return false;

    // Move current state to undo stack
    size_t len = 0;
    char* snapshot = getBufferSnapshot(file->buffer, &len);
    if (snapshot && len > 0 && stack->undoTop < MAX_UNDO_ENTRIES) {
        stack->undo[stack->undoTop].snapshot = snapshot;
        stack->undo[stack->undoTop].length = len;
        stack->undo[stack->undoTop].cursorRow = file->state.cursorRow;
        stack->undo[stack->undoTop].cursorCol = file->state.cursorCol;
        stack->undoTop++;
    }

    // Pop from redo
    stack->redoTop--;
    UndoEntry* entry = &stack->redo[stack->redoTop];

    IDECoreState* core = getCoreState();
    EditorView* activeEditorView = core->activeEditorView;


    loadSnapshotIntoBuffer(file->buffer, entry->snapshot, entry->length);
    if (activeEditorView && activeEditorView->activeTab >= 0 &&
                activeEditorView->openFiles[activeEditorView->activeTab] == file) {
        
        editorBuffer = file->buffer;
    }
    
    file->state.cursorRow = entry->cursorRow;
    file->state.cursorCol = entry->cursorCol;

    file->state.selecting = false;

    stack->pendingTextEdit = false;
    free(stack->typedWord);
    stack->typedWord = NULL;
    stack->typedWordLen = 0;
    stack->typedWordCap = 0;

    return true;
}


void clearUndoHistory(OpenFile* file) {
    if (!file) return;
    UndoStack* stack = getOrCreateUndoStack(file);
    if (!stack) return;

    for (int i = 0; i < stack->undoTop; i++) {
        free(stack->undo[i].snapshot);
    }
    for (int i = 0; i < stack->redoTop; i++) {
        free(stack->redo[i].snapshot);
    }

    stack->undoTop = 0;
    stack->redoTop = 0;
}







// 		STACK ACCESS
// 	==============================
// 		ACTIVE EDITS






void beginWordEdit(OpenFile* file, int row, int col) {
    if (!file) return;

    UndoStack* stack = getOrCreateUndoStack(file);
    if (!stack || stack->pendingTextEdit) return;

    stack->pendingTextEdit = true;
    stack->pendingStartRow = row;
    stack->pendingStartCol = col;

    stack->typedWordLen = 0;
    stack->typedWordCap = 32;

    stack->typedWord = malloc(stack->typedWordCap);
    if (stack->typedWord) {
        stack->typedWord[0] = '\0';  // ensure valid string
    }
}


void appendCharToWord(OpenFile* file, char ch) {
    if (!file) return;
    UndoStack* stack = getOrCreateUndoStack(file);
    if (!stack || !stack->pendingTextEdit) return;

    if (stack->typedWordLen + 1 >= stack->typedWordCap) {
        stack->typedWordCap *= 2;
        stack->typedWord = realloc(stack->typedWord, stack->typedWordCap);
    }

    stack->typedWord[stack->typedWordLen++] = ch;
    stack->typedWord[stack->typedWordLen] = '\0';
}


void commitWordEdit(OpenFile* file) {
    if (!file) return;
    UndoStack* stack = getOrCreateUndoStack(file);
    if (!stack) return;
    if (!stack->pendingTextEdit) return;

    // Only push if something was typed
    if (stack->typedWordLen > 0) {
        pushUndoState(file);
        printf("[UNDO] Committed word: \"%s\"\n", stack->typedWord);
    }

    stack->pendingTextEdit = false;

    free(stack->typedWord);
    stack->typedWord = NULL;
    stack->typedWordLen = 0;
    stack->typedWordCap = 0;
}
 
