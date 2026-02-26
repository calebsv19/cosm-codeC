#include "ide/Panes/Editor/editor_clipboard.h"
#include "editor.h"
#include "ide/Panes/Editor/undo_stack.h"
#include "ide/Panes/Editor/buffer_safety.h"
#include "core/Clipboard/clipboard.h"
#include <stdlib.h>
#include <string.h>

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

static void selection_buffer_store(const char* text) {
    if (!text) {
        selectionBuffer[0] = '\0';
        return;
    }
    size_t len = strlen(text);
    if (len >= MAX_SELECTION_LENGTH) {
        len = MAX_SELECTION_LENGTH - 1;
    }
    memcpy(selectionBuffer, text, len);
    selectionBuffer[len] = '\0';
}

static char* extract_selection_text(EditorBuffer* buffer, EditorState* state) {
    if (!state->selecting) return NULL;

    int startRow = state->selStartRow;
    int startCol = state->selStartCol;
    int endRow = state->cursorRow;
    int endCol = state->cursorCol;

    if (startRow > endRow || (startRow == endRow && startCol > endCol)) {
        int tmpRow = startRow, tmpCol = startCol;
        startRow = endRow; startCol = endCol;
        endRow = tmpRow;   endCol = tmpCol;
    }

    size_t total = 0;
    for (int r = startRow; r <= endRow; r++) {
        const char* line = buffer->lines[r];
        if (!line) line = "";
        int from = (r == startRow) ? startCol : 0;
        int to = (r == endRow) ? endCol : (int)strlen(line);
        if (from < 0) from = 0;
        if (to < from) to = from;
        total += (size_t)(to - from);
        if (r != endRow) total++; // newline
    }

    char* result = (char*)malloc(total + 1);
    if (!result) return NULL;

    size_t offset = 0;
    for (int r = startRow; r <= endRow; r++) {
        const char* line = buffer->lines[r];
        if (!line) line = "";
        int from = (r == startRow) ? startCol : 0;
        int to = (r == endRow) ? endCol : (int)strlen(line);
        if (from < 0) from = 0;
        if (to < from) to = from;
        size_t chunk = (size_t)(to - from);
        if (chunk > 0) {
            memcpy(result + offset, line + from, chunk);
            offset += chunk;
        }
        if (r != endRow) {
            result[offset++] = '\n';
        }
    }
    result[offset] = '\0';
    return result;
}

bool handleCommandCopySelection(EditorBuffer* buffer, EditorState* state) {
    if (!state->selecting) return false;
    char* text = extract_selection_text(buffer, state);
    if (!text) return false;

    selection_buffer_store(text);
    clipboard_copy_text(text);
    free(text);
    return true;
}  

bool handleCommandCutSelection(EditorBuffer* buffer, EditorState* state) {
    if (!state->selecting) return false;

    IDECoreState* core = getCoreState();
    EditorView* view = core->activeEditorView;
    if (!view || view->activeTab < 0 || view->activeTab >= view->fileCount) return false;
    OpenFile* file = view->openFiles[view->activeTab];
    if (!file) return false;

    pushUndoState(file);

    char* text = extract_selection_text(buffer, state);
    if (!text) return false;

    selection_buffer_store(text);
    clipboard_copy_text(text);

    // Now remove the selected text (shared logic)
    bool result = removeSelectedText(buffer, state);
    free(text);
    return result;
}



bool handleCommandPasteClipboard(EditorBuffer* buffer, EditorState* state) {
    char* clipboardText = clipboard_paste_text();
    const char* source = NULL;

    if (clipboardText && clipboardText[0] != '\0') {
        source = clipboardText;
    } else if (selectionBuffer[0] != '\0') {
        source = selectionBuffer;
    } else {
        clipboard_free_text(clipboardText);
        return false;
    }

    IDECoreState* core = getCoreState();
    EditorView* view = core->activeEditorView;
    if (!view || view->activeTab < 0 || view->activeTab >= view->fileCount) return false;
    OpenFile* file = view->openFiles[view->activeTab];
    if (!file) return false;

    pushUndoState(file);

    int requiredExtraLines = 0;
    for (const char* p = source; *p; ++p) {
        if (*p == '\n') requiredExtraLines++;
    }
    if (!ensure_line_capacity(buffer, requiredExtraLines)) {
        clipboard_free_text(clipboardText);
        return false;
    }

    // 🔁 NEW: If selection exists, delete it first (and move cursor)
    if (state->selecting) {
	removeSelectedText(buffer, state);
    }

    char* currentLine = buffer->lines[state->cursorRow];
    char* originalLine = currentLine;
    int currentLen = strlen(currentLine);
    int insertPos = state->cursorCol;

    char* beforeCursor = malloc(insertPos + 1);
    char* afterCursor  = malloc(currentLen - insertPos + 1);
    if (!beforeCursor || !afterCursor) return false;

    strncpy(beforeCursor, currentLine, insertPos);
    beforeCursor[insertPos] = '\0';
    strcpy(afterCursor, currentLine + insertPos);

    char* selectionCopy = strdup(source);
    if (!selectionCopy) {
        clipboard_free_text(clipboardText);
        free(beforeCursor);
        free(afterCursor);
        return false;
    }
    free(originalLine);

    char* line = strtok(selectionCopy, "\n");
    int row = state->cursorRow;

    if (line) {
        char* newLine = malloc(strlen(beforeCursor) + strlen(line) + 1);
        sprintf(newLine, "%s%s", beforeCursor, line);
        buffer->lines[row] = newLine;
    }

    while ((line = strtok(NULL, "\n")) != NULL) {
        for (int i = buffer->lineCount; i > row + 1; i--) {
            buffer->lines[i] = buffer->lines[i - 1];
        }
        buffer->lines[row + 1] = strdup(line);
        buffer->lineCount++;
        row++;
    }

    char* lastLine = buffer->lines[row];
    char* merged = malloc(strlen(lastLine) + strlen(afterCursor) + 1);
    sprintf(merged, "%s%s", lastLine, afterCursor);
    free(buffer->lines[row]);
    buffer->lines[row] = merged;

    state->cursorRow = row;
    state->cursorCol = strlen(buffer->lines[row]) - strlen(afterCursor);

    free(beforeCursor);
    free(afterCursor);
    free(selectionCopy);
    if (source != selectionBuffer) {
        selection_buffer_store(source);
    }
    clipboard_free_text(clipboardText);

    return true;
}


bool handleCommandTextClipboard(SDL_Keycode key, EditorBuffer* buffer, EditorState* state) {
    switch (key) {
        case SDLK_c:
            return handleCommandCopySelection(buffer, state);
            
        case SDLK_x:
            return handleCommandCutSelection(buffer, state);
            
        case SDLK_v:
            return handleCommandPasteClipboard(buffer, state);
            
        default:
            return false;
    }
}

bool removeSelectedText(EditorBuffer* buffer, EditorState* state) {
    if (!state->selecting) return false;

    int startRow = state->selStartRow, startCol = state->selStartCol;
    int endRow = state->cursorRow, endCol = state->cursorCol;

    if (startRow > endRow || (startRow == endRow && startCol > endCol)) {
        int tmpRow = startRow, tmpCol = startCol;
        startRow = endRow; startCol = endCol;
        endRow = tmpRow;   endCol = tmpCol;
    }

    if (startRow == endRow) {
        char* line = buffer->lines[startRow];
        int len = strlen(line);
        memmove(line + startCol, line + endCol, len - endCol + 1);
    } else {
        char* startLine = buffer->lines[startRow];
        char* endLine   = buffer->lines[endRow];

        int startLen = startCol;
        int endLen   = strlen(endLine) - endCol;

        char* merged = malloc(startLen + endLen + 1);
        if (!merged) return false;
        strncpy(merged, startLine, startCol);
        strcpy(merged + startCol, endLine + endCol);

        free(buffer->lines[startRow]);
        buffer->lines[startRow] = merged;

        for (int i = startRow + 1; i <= endRow; i++) {
            free(buffer->lines[i]);
        }
        for (int i = endRow + 1; i < buffer->lineCount; i++) {
            buffer->lines[i - (endRow - startRow)] = buffer->lines[i];
        }

        buffer->lineCount -= (endRow - startRow);
    }

    state->cursorRow = startRow;
    state->cursorCol = startCol;
    state->selecting = false;

    enforceNonEmptyBuffer(buffer);
    return true;
}





//              Text Buffer
//      ===============================
//              Line Buffer




static bool handleCommandDuplicateLine(EditorView* view, EditorBuffer* buffer, EditorState* state) {
    OpenFile* file = view->openFiles[view->activeTab];
    if (!file) return false;
    
    commitWordEdit(file);
    pushUndoState(file);
     
    if (!ensure_line_capacity(buffer, 1)) return false;
    
    int row = state->cursorRow;
    const char* src = buffer->lines[row];
    if (!src) return false;
    
    char* duplicate = strdup(src);
    if (!duplicate) return false;
    
    for (int i = buffer->lineCount; i > row; i--) {
        buffer->lines[i] = buffer->lines[i - 1];
    }   
            
    buffer->lines[row + 1] = duplicate;
    buffer->lineCount++;
    state->cursorRow++;
    
    markFileAsModified(file);
    return true;
}


static bool handleCommandCutLine(EditorView* view, EditorBuffer* buffer, EditorState* state) {
    OpenFile* file = view->openFiles[view->activeTab];
    if (!file) return false;
    
    commitWordEdit(file);
    pushUndoState(file); 
    
    if (buffer->lineCount <= 0) return false;
    
    int row = state->cursorRow;
    pushCutLine(buffer->lines[row]);
    
    free(buffer->lines[row]);
    for (int i = row; i < buffer->lineCount - 1; i++) {
        buffer->lines[i] = buffer->lines[i + 1];
    }
     
    buffer->lineCount--;
    state->cursorRow = (state->cursorRow >= buffer->lineCount)
        ? buffer->lineCount - 1 : state->cursorRow;
    if (state->cursorRow < 0) state->cursorRow = 0;
    state->cursorCol = 0;
    
    markFileAsModified(file);
    enforceNonEmptyBuffer(buffer);
    return true;
}


static bool handleCommandPasteCutBuffer(EditorView* view, EditorBuffer* buffer, EditorState* state) {
    OpenFile* file = view->openFiles[view->activeTab];
    if (!file) return false;
    
    commitWordEdit(file);
    pushUndoState(file);
                        
    if (editorCutBuffer.count == 0 ||
        !ensure_line_capacity(buffer, editorCutBuffer.count))
        return false;
             
    int row = state->cursorRow;
    for (int i = buffer->lineCount - 1; i > row - 1; i--) {
        buffer->lines[i + editorCutBuffer.count] = buffer->lines[i];
    }
    
    for (int j = 0; j < editorCutBuffer.count; j++) {
        buffer->lines[row + j + 1] = strdup(editorCutBuffer.cutLines[j]);
    }
    
    buffer->lineCount += editorCutBuffer.count;
    state->cursorRow += editorCutBuffer.count;
    state->cursorCol = 0;
    
    markFileAsModified(file);
    return true;
}
        
static bool handleCommandClearCutBuffer(void) {
    clearCutBuffer();
    return true;
}
 
static bool handleCommandSaveIfModified(EditorView* view) {
    if (!view || view->type != VIEW_LEAF || view->activeTab < 0 || view->activeTab >= view->fileCount)
        return false;
        
    OpenFile* file = view->openFiles[view->activeTab];
    if (!file) return false;
        
    if (file->isModified) {
     
        if (view && view->fileCount > 0 && view->activeTab >= 0
                        && view->activeTab < view->fileCount) {
            OpenFile* file = view->openFiles[view->activeTab];
            if (file) {
                enqueueSave(file);
            }
        }
 
        enqueueSave(file);
        printf("[Command] Save triggered for: %s\n", file->filePath);
    } else {
        printf("[Command] Save skipped — file not modified: %s\n", file->filePath);
    }
    
    return true;
}




bool handleCommandLineClipboard(SDL_Keycode key, EditorBuffer* buffer, EditorState* state) {
    IDECoreState* core = getCoreState();
    EditorView* activeEditorView = core->activeEditorView;
        
    switch (key) {
        case SDLK_p:
            return handleCommandDuplicateLine(activeEditorView, buffer, state);
 
        case SDLK_k:
            return handleCommandCutLine(activeEditorView, buffer, state);
 
        case SDLK_u:
            return handleCommandPasteCutBuffer(activeEditorView, buffer, state);
 
        case SDLK_i:
            return handleCommandClearCutBuffer();

        case SDLK_o:
            return handleCommandSaveIfModified(activeEditorView);
     
        default:
            return false;
    }
}


//              Line Buffer
//      ===============================
//              HELPERS


const char* peekLastCutLine(void) {
    if (editorCutBuffer.count == 0) return NULL;
    return editorCutBuffer.cutLines[editorCutBuffer.count - 1];
}


void pushCutLine(const char* line) {
    if (!line || !*line) return;
    if (editorCutBuffer.count >= MAX_CUT_BUFFER) return;

    editorCutBuffer.cutLines[editorCutBuffer.count++] = strdup(line);
}

void clearCutBuffer(void) {
    for (int i = 0; i < editorCutBuffer.count; i++) {
        free(editorCutBuffer.cutLines[i]);
        editorCutBuffer.cutLines[i] = NULL;
    }
    editorCutBuffer.count = 0;
}
