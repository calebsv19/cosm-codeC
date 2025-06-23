#include "Editor/editor_text_edit.h"
#include "Editor/undo_stack.h"


void handleCommandInsertNewline(OpenFile* file, EditorBuffer* buffer, EditorState* state) {
    commitWordEdit(file);
    pushUndoState(file);
    handleReturnKey(buffer, state);  // keep this as your newline logic
}   
        
void handleCommandDeleteCharacter(OpenFile* file, EditorBuffer* buffer, EditorState* state) {
    pushUndoState(file);
    handleBackspaceKey(buffer, state);  // keep your backspace handler here
}
    
void handleCommandInsertCharacter(OpenFile* file, EditorBuffer* buffer,
                                         EditorState* state, char ch) {
    int row = state->cursorRow;
    int col = state->cursorCol;
    if (row < 0 || row >= buffer->lineCount) return;
    
    char* oldLine = buffer->lines[row];
    if (!oldLine) return;
    
    int oldLen = strlen(oldLine);
    char* newLine = malloc(oldLen + 2); // +1 for new char, +1 for null
    if (!newLine) return;
        
    strncpy(newLine, oldLine, col);
    newLine[col] = ch;
    strcpy(newLine + col + 1, oldLine + col);
    
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
    int row = state->cursorRow;
    int col = state->cursorCol;
    
    if (row < 0 || row >= buffer->lineCount) return;
    char* line = buffer->lines[row];
    
    IDECoreState* core = getCoreState();
    EditorView* view = core->activeEditorView;
    
    if (!view || view->type != VIEW_LEAF || view->activeTab < 0 || view->activeTab >= view->fileCount)
        return;
        
    OpenFile* file = view->openFiles[view->activeTab];
    
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
}


void handleReturnKey(EditorBuffer* buffer, EditorState* state) {
    int row = state->cursorRow;
    int col = state->cursorCol;
    
    if (row < 0 || row >= buffer->lineCount) return;
    char* current = buffer->lines[row];
    int currentLen = strlen(current);  
    
    char* before = malloc(col + 1);
    char* after = malloc(currentLen - col + 1);
    if (!before || !after) return;
    
    strncpy(before, current, col);
    before[col] = '\0';
    strcpy(after, current + col);
    
    free(buffer->lines[row]);
    buffer->lines[row] = before;
    
    IDECoreState* core = getCoreState();
    EditorView* view = core->activeEditorView;
    
    if (!view || view->type != VIEW_LEAF || view->activeTab < 0 || view->activeTab >= view->fileCount) 
{
        free(after);
        return;
    }
     
    OpenFile* file = view->openFiles[view->activeTab];
    
    if (buffer->lineCount < buffer->capacity) {
        for (int i = buffer->lineCount; i > row + 1; i--) {
            buffer->lines[i] = buffer->lines[i - 1];
        }
        buffer->lines[row + 1] = after;
        buffer->lineCount++;
        state->cursorRow++; 
        state->cursorCol = 0;
        markFileAsModified(file);
    } else {
        buffer->lines[row] = realloc(buffer->lines[row], currentLen + 1);
        strcat(buffer->lines[row], after);
        free(after);
    }
}
