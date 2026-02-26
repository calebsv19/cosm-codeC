#include "ide/Panes/Editor/editor_text_edit.h"
#include "ide/Panes/Editor/undo_stack.h"
#include "ide/Panes/Editor/buffer_safety.h" 
#include "ide/Panes/Editor/editor_clipboard.h"
#include <string.h>
#include <stdlib.h>

static bool ensure_line_capacity(EditorBuffer* buffer, int additional_lines) {
    if (!buffer || additional_lines <= 0) return true;
    if (buffer->lineCount + additional_lines <= buffer->capacity) return true;

    int newCapacity = (buffer->capacity > 0) ? buffer->capacity : INITIAL_CAPACITY;
    while (newCapacity < buffer->lineCount + additional_lines) {
        if (newCapacity > (1 << 28)) return false;
        newCapacity *= 2;
    }

    char** grown = (char**)realloc(buffer->lines, sizeof(char*) * (size_t)newCapacity);
    if (!grown) return false;
    buffer->lines = grown;
    buffer->capacity = newCapacity;
    return true;
}

void handleCommandInsertNewline(OpenFile* file, EditorBuffer* buffer, EditorState* state) {
    commitWordEdit(file);
    pushUndoState(file);
    handleReturnKey(buffer, state);  // keep this as your newline logic
}   
        
void handleCommandDeleteCharacter(OpenFile* file, EditorBuffer* buffer, EditorState* state) {
    pushUndoState(file);
    handleBackspaceKey(buffer, state);  // keep your backspace handler here

    enforceNonEmptyBuffer(buffer);
}
    
void handleCommandDeleteForward(OpenFile* file, EditorBuffer* buffer, EditorState* state) {
    if (!file || !buffer || !state) return;

    if (state->selecting) {
        pushUndoState(file);
        removeSelectedText(buffer, state);
        markFileAsModified(file);
        enforceNonEmptyBuffer(buffer);
        return;
    }

    int row = state->cursorRow;
    int col = state->cursorCol;
    if (row < 0 || row >= buffer->lineCount) return;

    char* line = buffer->lines[row];
    if (!line) line = "";
    int len = strlen(line);

    if (col < len) {
        pushUndoState(file);
        memmove(line + col, line + col + 1, (size_t)(len - col));
        markFileAsModified(file);
        enforceNonEmptyBuffer(buffer);
        return;
    }

    if (row < buffer->lineCount - 1) {
        char* nextLine = buffer->lines[row + 1];
        if (!nextLine) nextLine = "";
        size_t nextLen = strlen(nextLine);
        char* merged = malloc((size_t)len + nextLen + 1);
        if (!merged) return;

        strcpy(merged, line);
        strcat(merged, nextLine);

        pushUndoState(file);
        free(buffer->lines[row]);
        free(buffer->lines[row + 1]);
        buffer->lines[row] = merged;
        for (int i = row + 1; i < buffer->lineCount - 1; ++i) {
            buffer->lines[i] = buffer->lines[i + 1];
        }
        buffer->lines[buffer->lineCount - 1] = NULL;
        buffer->lineCount--;

        markFileAsModified(file);
        enforceNonEmptyBuffer(buffer);
        return;
    }
}

void handleCommandInsertCharacter(OpenFile* file, EditorBuffer* buffer,
                                         EditorState* state, char ch) {
    if (!file || !buffer || !state) return;
    int row = state->cursorRow;
    int col = state->cursorCol;
    if (row < 0 || row >= buffer->lineCount) return;
    
    char* oldLine = buffer->lines[row];
    if (!oldLine) return;
    
    int oldLen = (int)strlen(oldLine);
    if (col < 0) col = 0;
    if (col > oldLen) col = oldLen;
    state->cursorCol = col;

    char* newLine = (char*)malloc((size_t)oldLen + 2u); // +1 for new char, +1 for null
    if (!newLine) return;

    if (col > 0) {
        memcpy(newLine, oldLine, (size_t)col);
    }
    newLine[col] = ch;
    memcpy(newLine + col + 1, oldLine + col, (size_t)(oldLen - col) + 1u);
    
    free(buffer->lines[row]);  
    buffer->lines[row] = newLine;
            
    state->cursorCol++;
        
    markFileAsModified(file);
        
    if (ch == ' ') {
        commitWordEdit(file); 
    } else {
        beginWordEdit(file, state->cursorRow, state->cursorCol);
        appendCharToWord(file, ch);
    }
}


void handleCommandCharacterInput(SDL_Event* event, EditorBuffer* buffer, EditorState* state) {
    SDL_Keycode key = event->key.keysym.sym;
    
    IDECoreState* core = getCoreState();
    EditorView* view = core->activeEditorView;
    
    if (!view || view->type != VIEW_LEAF ||
        view->activeTab < 0 || view->activeTab >= view->fileCount) return;
        
    OpenFile* file = view->openFiles[view->activeTab];
    if (!file) return;
    
    switch (key) {
        case SDLK_RETURN:
            handleCommandInsertNewline(file, buffer, state);
            return;
            
        case SDLK_BACKSPACE:
            handleCommandDeleteCharacter(file, buffer, state);
            return;
        case SDLK_DELETE:
            handleCommandDeleteForward(file, buffer, state);
            return;
            
        default:
            break;
    }
     
    if (key < 32 || key >= 127) return;
    
    handleCommandInsertCharacter(file, buffer, state, (char)key);
}


//              CHAR INPUT
//====================================================
//              TEXT EDITS



void handleBackspaceKey(EditorBuffer* buffer, EditorState* state) {
    if (!buffer || !state) return;

    IDECoreState* core = getCoreState();
    EditorView* view = core->activeEditorView;
    if (!view || view->type != VIEW_LEAF ||
        view->activeTab < 0 || view->activeTab >= view->fileCount) return;

    OpenFile* file = view->openFiles[view->activeTab];
    if (!file) return;

    // ✅ If a selection exists, delete the full region instead of just one char
    if (state->selecting) {
        pushUndoState(file);
        removeSelectedText(buffer, state);
        markFileAsModified(file);
        return;
    }

    // === Existing backspace logic ===
    int row = state->cursorRow;
    int col = state->cursorCol;

    if (row < 0 || row >= buffer->lineCount) return;
    char* line = buffer->lines[row];

    if (col > 0) {
        int len = strlen(line);
        for (int i = col - 1; i < len; i++) {
            line[i] = line[i + 1];
        }
        state->cursorCol--;
        markFileAsModified(file);

    } else if (row > 0) {
        int prevRow = row - 1;
        char* prevLine = buffer->lines[prevRow];
        int prevLen = strlen(prevLine);
        int currLen = strlen(line);

        char* merged = malloc(prevLen + currLen + 1);
        if (!merged) return;

        strcpy(merged, prevLine);
        strcat(merged, line);

        free(buffer->lines[prevRow]);
        free(buffer->lines[row]);
        buffer->lines[prevRow] = merged;

        for (int i = row; i < buffer->lineCount - 1; i++) {
            buffer->lines[i] = buffer->lines[i + 1];
        }

        buffer->lines[buffer->lineCount - 1] = NULL;
        buffer->lineCount--;

        state->cursorRow--;
        state->cursorCol = prevLen;
        markFileAsModified(file);
    }

    enforceNonEmptyBuffer(buffer);
}


void handleReturnKey(EditorBuffer* buffer, EditorState* state) {
    int row = state->cursorRow;
    int col = state->cursorCol;
    
    if (row < 0 || row >= buffer->lineCount) return;
    char* current = buffer->lines[row];
    int currentLen = strlen(current);  
    
    if (col < 0) col = 0;
    if (col > currentLen) col = currentLen;

    char* before = (char*)malloc((size_t)col + 1u);
    char* after = (char*)malloc((size_t)(currentLen - col) + 1u);
    if (!before || !after) {
        free(before);
        free(after);
        return;
    }
    
    strncpy(before, current, col);
    before[col] = '\0';
    strcpy(after, current + col);
    
    IDECoreState* core = getCoreState();
    EditorView* view = core->activeEditorView;
    
    if (!view || view->type != VIEW_LEAF || view->activeTab < 0 || view->activeTab >= view->fileCount) 
{
        free(before);
        free(after);
        return;
    }
     
    OpenFile* file = view->openFiles[view->activeTab];
    if (!ensure_line_capacity(buffer, 1)) {
        free(before);
        free(after);
        return;
    }

    free(buffer->lines[row]);
    buffer->lines[row] = before;

    for (int i = buffer->lineCount; i > row + 1; i--) {
        buffer->lines[i] = buffer->lines[i - 1];
    }
    buffer->lines[row + 1] = after;
    buffer->lineCount++;
    state->cursorRow++;
    state->cursorCol = 0;
    markFileAsModified(file);
}
