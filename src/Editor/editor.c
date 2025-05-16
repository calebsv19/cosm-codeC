
#include "editor.h"
#include "editor_view.h"  // for EditorView and OpenFile
#include "editor_state.h"
#include "Render/render_text_helpers.h"
#include "undo_stack.h"

#include "CommandBus/command_bus.h"
#include "GlobalInfo/core_state.h"
#include "pane.h"
#include "GlobalInfo/system_control.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define clamp(v, lo, hi) ((v) < (lo) ? (lo) : ((v) > (hi) ? (hi) : (v)))



CutBuffer editorCutBuffer = {
    .cutLines = {NULL},
    .count = 0
};

char selectionBuffer[MAX_SELECTION_LENGTH] = {0};




EditorBuffer* editorBuffer = NULL; // GLOBAL storage
EditorState* editorState = NULL;










// 		INITS
// ============================================
//              CURSOR LOGIC







// Safe helper for checking if a line contains only whitespace
bool isLineWhitespaceOnly(const char* line) {
    if (!line) return true;
    while (*line) {
        if (*line != ' ' && *line != '\t' && *line != '\r' && *line != '\n')
            return false;
        line++;
    }
    return true;
}


bool isCursorInSelection(EditorState* state) {
    return state->selecting;
}


int getVisibleEditorLineCount(UIPane* pane) {
    // Leave room for header/margins — adjust as needed
    int lineHeight = 20;
    int paddingTop = 30;
    return (pane->h - paddingTop) / lineHeight;
} 

bool isLineInSelection(int row, int* startCol, int* endCol, EditorBuffer* buffer, EditorState* state){
    if (!state->selecting) return false;
    
    int sr = state->selStartRow;
    int sc = state->selStartCol;
    int cr = state->cursorRow;  
    int cc = state->cursorCol;  
    
    // Normalize range
    if (sr > cr || (sr == cr && sc > cc)) {
        int tmpRow = sr, tmpCol = sc;
        sr = cr; sc = cc;
        cr = tmpRow; cc = tmpCol;
    }
     
    if (row < sr || row > cr) return false;
    
    if (row == sr && row == cr) {
        *startCol = sc;
        *endCol = cc;  
    } else if (row == sr) {
        *startCol = sc;
        *endCol = strlen(buffer->lines[row]);
    } else if (row == cr) {
        *startCol = 0;
        *endCol = cc; 
    } else {
        *startCol = 0;
        *endCol = strlen(buffer->lines[row]);
    }
     
    return true;
}




// 	======================================
//		SCROLL WHEEL







static void handleCommandEditorScroll(EditorState* state, EditorBuffer* buffer, EditorView* view, int scrollDelta) {
    const int lineHeight = 20;
    const int contentY = view->y + HEADER_HEIGHT + 2;
    const int contentH = view->h - (contentY - view->y);
    const int maxVisibleLines = (contentH > 0) ? contentH / lineHeight : 0;

    state->viewTopRow -= scrollDelta;
    if (state->viewTopRow < 0) state->viewTopRow = 0;

    int maxScroll = buffer->lineCount - maxVisibleLines;
    if (state->viewTopRow > maxScroll) state->viewTopRow = maxScroll;

    // Align cursor with view
    state->cursorRow -= scrollDelta;
    if (state->cursorRow < 0) state->cursorRow = 0;
    if (state->cursorRow >= buffer->lineCount) state->cursorRow = buffer->lineCount - 1;
}


bool handleEditorScrollWheel(UIPane* pane, SDL_Event* event) {
    if (event->type != SDL_MOUSEWHEEL)
        return false;

    IDECoreState* core = getCoreState();
    EditorView* view = core->activeEditorView;

    if (!view || view->type != VIEW_LEAF || view->fileCount <= 0)
        return false;

    if (view->activeTab < 0 || view->activeTab >= view->fileCount)
        return false;

    OpenFile* file = view->openFiles[view->activeTab];
    if (!file || !file->buffer)
        return false;

    EditorState* state = &file->state;
    EditorBuffer* buffer = file->buffer;

    int scrollDelta = event->wheel.y;

    handleCommandEditorScroll(state, buffer, view, scrollDelta);
    return true;
}






//              SCROLL WHEEL
//      =======================================
//              SCROLLBAR EVENT








static bool handleCommandEditorScrollDrag(EditorState* state, EditorBuffer* buffer, int mouseY,
                                          int scrollbarY, int scrollbarH, int thumbH, int maxVisibleLines) {
    if (mouseY == state->scrollbarStartY) {
        return true;  // No movement yet
    }

    int maxScroll = buffer->lineCount - maxVisibleLines;
    float maxThumbTravel = (float)(scrollbarH - thumbH);

    int thumbTop = mouseY - state->scrollbarDragOffsetY;
    thumbTop = clamp(thumbTop, scrollbarY, scrollbarY + scrollbarH - thumbH);
    float thumbProgress = (float)(thumbTop - scrollbarY) / maxThumbTravel;

    int newTopRow = (int)(thumbProgress * maxScroll);
    newTopRow = clamp(newTopRow, -1, maxScroll);

    if (newTopRow != state->viewTopRow) {
        printf("[SCROLL] viewTopRow updated: %d → %d\n", state->viewTopRow, newTopRow);
    }

    state->viewTopRow = newTopRow;
    state->cursorRow = clamp(state->cursorRow, state->viewTopRow,
                             state->viewTopRow + maxVisibleLines - 1);

    return true;
}

static bool handleCommandEditorScrollRelease(void) {
    endEditorScrollbarDrag();
    return true;
}



bool handleEditorScrollbarEvent(UIPane* pane, SDL_Event* event) {
    if (!pane || !pane->editorView) return false;

    int mouseX, mouseY;
    SDL_GetMouseState(&mouseX, &mouseY);

    EditorView* view = findLeafUnderCursor(pane->editorView, mouseX, mouseY);
    if (!view || view->type != VIEW_LEAF || view->fileCount <= 0) return false;

    OpenFile* file = view->openFiles[view->activeTab];
    if (!file || !file->buffer) return false;

    EditorBuffer* buffer = file->buffer;
    EditorState* state = &file->state;

    const int lineHeight = 20;
    const int contentY = pane->y + HEADER_HEIGHT + 2;
    const int contentH = pane->h - (contentY - pane->y);
    if (contentH <= 0) return false;

    const int maxVisibleLines = contentH / lineHeight;
    const int maxScroll = buffer->lineCount - maxVisibleLines;
    if (maxScroll <= 0) return false;

    const int scrollbarY = contentY;
    const int scrollbarH = contentH;

    int thumbH = (int)((float)maxVisibleLines / buffer->lineCount * scrollbarH);
    if (thumbH < 20) thumbH = 20;

    state->selecting = false;

    // === Drag Scroll Behavior ===
    if (event->type == SDL_MOUSEMOTION && getDraggingEditorPane() == pane) {
        return handleCommandEditorScrollDrag(state, buffer, event->motion.y, scrollbarY, 
						scrollbarH, thumbH, maxVisibleLines);
    }

    // === End Drag Behavior ===
    if (event->type == SDL_MOUSEBUTTONUP && event->button.button == SDL_BUTTON_LEFT) {
        if (getDraggingEditorPane() == pane) {
            return handleCommandEditorScrollRelease();
        }
    }

    return false;
}





//              SCROLL BAR
//      =======================================
//              KEY DOWN







static void beginTextSelectionIfNeeded(EditorState* state) {
    if (!state->selecting) {
        state->selecting = true;
        state->selStartRow = state->cursorRow;
        state->selStartCol = state->cursorCol;
    }
}

static bool isArrowKey(SDL_Keycode key) {
    return key == SDLK_LEFT || key == SDLK_RIGHT ||
           key == SDLK_UP || key == SDLK_DOWN;
}

static void handleCommandSwitchTab(int direction) {
    EditorView* view = getCoreState()->activeEditorView;
    if (view) switchTab(view, direction);
}

static void handleCommandMoveCursor(SDL_Keycode key, EditorBuffer* buffer, EditorState* state, int paneHeight) {
    handleArrowKeyPress(key, buffer, state, paneHeight);  // existing function
}



void handleEditorKeyDown(SDL_Event* event, EditorView* view, UIPane* pane) {
    if (!view || view->type != VIEW_LEAF || view->fileCount <= 0) return;

    OpenFile* file = view->openFiles[view->activeTab];
    if (!file || !file->buffer) return;

    EditorBuffer* buffer = file->buffer;
    EditorState* state = &file->state;

    SDL_Keymod mod = SDL_GetModState();
    SDL_Keycode key = event->key.keysym.sym;

    printf("Editor received key event: %d (%s)\n", key, SDL_GetKeyName(key));

    bool shiftHeld = (mod & KMOD_SHIFT);
    bool cmdHeld   = (mod & KMOD_GUI);
    bool altHeld   = (mod & KMOD_ALT);

    if ((shiftHeld || cmdHeld) &&
        (key == SDLK_LEFT || key == SDLK_RIGHT || key == SDLK_UP || key == SDLK_DOWN)) {
        beginTextSelectionIfNeeded(state);
    }

    if (isArrowKey(key) || key == SDLK_HOME || key == SDLK_END) {
        handleCommandMoveCursor(key, buffer, state, pane->h);
        if (!shiftHeld) state->selecting = false;
        return;
    }

    if (cmdHeld) {
        if (key == SDLK_1) {
            handleCommandSwitchTab(+1);
            return;
        }
        handleCommandMovement(key, buffer, state, getCoreState()->activeEditorView, pane->h);
        handleCommandAction(key, buffer, state, view, pane);
    } else if (shiftHeld && isSpecialShiftAction(key)) {
        handleCommandShiftAction(key, buffer, state);
    } else if (altHeld) {
        handleCommandAltAction(key, buffer, state);
    } else {
        handleCommandCharacterInput(event, buffer, state);
    }
}






//              KEY DOWN       
//      =======================================
//		ARROW KEY PRESS    






static void handleMoveCursorUp(EditorBuffer* buffer, EditorState* state) {
    if (state->cursorRow > 0) {
        state->cursorRow--;
        int prevLen = strlen(buffer->lines[state->cursorRow]);
        if (state->cursorCol > prevLen)
            state->cursorCol = prevLen;
    }
}

static void handleMoveCursorDown(EditorBuffer* buffer, EditorState* state) {
    if (state->cursorRow < buffer->lineCount - 1) {
        state->cursorRow++;
        int nextLen = strlen(buffer->lines[state->cursorRow]);
        if (state->cursorCol > nextLen)
            state->cursorCol = nextLen;
    }
}

static void handleMoveCursorLeft(EditorBuffer* buffer, EditorState* state) {
    if (state->cursorCol > 0) {
        state->cursorCol--;
    } else if (state->cursorRow > 0) {
        state->cursorRow--;
        state->cursorCol = strlen(buffer->lines[state->cursorRow]);
    }
}

static void handleMoveCursorRight(EditorBuffer* buffer, EditorState* state) {
    const char* line = buffer->lines[state->cursorRow];
    int lineLen = strlen(line);
    if (state->cursorCol < lineLen) {
        state->cursorCol++;
    } else if (state->cursorRow < buffer->lineCount - 1) {
        state->cursorRow++;
        state->cursorCol = 0;
    }
}

static void handleMoveCursorToStartOfLine(EditorBuffer* buffer, EditorState* state) {
    (void)buffer;  // unused
    state->cursorCol = 0;
}

static void handleMoveCursorToEndOfLine(EditorBuffer* buffer, EditorState* state) {
    state->cursorCol = strlen(buffer->lines[state->cursorRow]);
}



void handleArrowKeyPress(SDL_Keycode key, EditorBuffer* buffer, EditorState* state, int paneHeight) {
    printf("Arrow key pressed: %d\n", key);  // <- TEMP DEBUG

    switch (key) {
        case SDLK_UP:
            handleMoveCursorUp(buffer, state);
            break;

        case SDLK_DOWN:
            handleMoveCursorDown(buffer, state);
            break;

        case SDLK_LEFT:
            handleMoveCursorLeft(buffer, state);
            break;

        case SDLK_RIGHT:
            handleMoveCursorRight(buffer, state);
            break;

        case SDLK_HOME:
            handleMoveCursorToStartOfLine(buffer, state);
            break;

        case SDLK_END:
            handleMoveCursorToEndOfLine(buffer, state);
            break;
    }
}





//              ARROW KEY PRESS
//      =======================================
//              COMMAND MOVEMENT








void handleCommandMovement(SDL_Keycode key, EditorBuffer* buffer, EditorState* state, 
				EditorView* view, int paneHeight) {

    if (!buffer || buffer->lineCount == 0) return;

    SDL_Keymod mod = SDL_GetModState();
    bool shiftHeld = mod & KMOD_SHIFT;

    if (handleCommandNavigation(key, buffer, state, paneHeight, shiftHeld)) return;
    if (handleCommandLineClipboard(key, buffer, state)) return;
    if (handleCommandTextClipboard(key, buffer, state)) return;

}








static void handleCommandAddEditorView(UIPane* pane) {
    printf("[COMMAND] Add new editor view requested.\n");
    EditorView* root = getCoreState()->editorPane->editorView;
    addEditorView(root, pane);  // Always split from root
}

static void handleCommandSwitchTabWithMod(void) {
    SDL_Keymod mod = SDL_GetModState();
    int direction = (mod & KMOD_SHIFT) ? -1 : 1;
    switchTab(getCoreState()->activeEditorView, direction);
}

void handleCommandAction(SDL_Keycode key, EditorBuffer* buffer, EditorState* state,
                         EditorView* view, UIPane* pane) {
    if (key == SDLK_e) {
        handleCommandAddEditorView(pane);
    }

    if (key == SDLK_TAB) {
        handleCommandSwitchTabWithMod();
    }

    // More command key mappings will be added here
}









static bool handleCommandJumpWordLeft(const char* line, EditorState* state) {
    int i = state->cursorCol;
    while (i > 0 && line[i - 1] == ' ') i--;
    while (i > 0 && line[i - 1] != ' ') i--;
    state->cursorCol = i;
    return true;
}

static bool handleCommandJumpWordRight(const char* line, EditorState* state) {
    int i = state->cursorCol;
    while (line[i] && line[i] != ' ') i++;
    while (line[i] == ' ') i++;
    state->cursorCol = i;
    return true;
}

static bool handleCommandSkipParagraph(EditorBuffer* buffer, EditorState* state, int direction, int maxVisibleLines) {
    int row = state->cursorRow + direction;

    while (row > 0 && row < buffer->lineCount - 1 &&
           !isLineWhitespaceOnly(buffer->lines[row])) {
        row += direction;
    }

    if (direction > 0 && row < buffer->lineCount - 1) row++;
    if (direction < 0 && row > 0) row--;

    state->cursorRow = clamp(row, 0, buffer->lineCount - 1);

    const char* newLine = buffer->lines[state->cursorRow];
    int newLen = newLine ? strlen(newLine) : 0;
    if (state->cursorCol > newLen) state->cursorCol = newLen;

    state->viewTopRow = state->cursorRow - maxVisibleLines / 2;
    if (state->viewTopRow < 0) state->viewTopRow = 0;

    return true;
}

static bool handleCommandJumpToTop(EditorState* state) {
    state->cursorRow = 0;
    state->cursorCol = 0;
    state->viewTopRow = 0;
    return true;
}

static bool handleCommandJumpToBottom(EditorBuffer* buffer, EditorState* state, int maxVisibleLines) {
    state->cursorRow = buffer->lineCount - 1;
    state->cursorCol = strlen(buffer->lines[state->cursorRow]);
    state->viewTopRow = state->cursorRow - maxVisibleLines + 1;
    if (state->viewTopRow < 0) state->viewTopRow = 0;
    return true;
}

static bool handleCommandTrimLineStart(const char* line, EditorState* state) {
    while (state->cursorCol > 0 && line[state->cursorCol - 1] == ' ')
        state->cursorCol--;
    state->cursorCol = 0;
    return true;
}



bool handleCommandNavigation(SDL_Keycode key, EditorBuffer* buffer, EditorState* state,
                             int paneHeight, bool shiftHeld) {
    int lineHeight = 20;
    int maxVisibleLines = (paneHeight - 30) / lineHeight;

    const char* line = buffer->lines[state->cursorRow];
    if (!line) return false;
    int lineLen = strlen(line);
    if (state->cursorCol > lineLen) state->cursorCol = lineLen;

    if (shiftHeld && !state->selecting) {
        state->selecting = true;
        state->selStartRow = state->cursorRow;
        state->selStartCol = state->cursorCol;
    }

    switch (key) {
        case SDLK_LEFT:
            return handleCommandJumpWordLeft(line, state);

        case SDLK_RIGHT:
            return handleCommandJumpWordRight(line, state);

        case SDLK_UP:
            return handleCommandSkipParagraph(buffer, state, -1, maxVisibleLines);

        case SDLK_DOWN:
            return handleCommandSkipParagraph(buffer, state, +1, maxVisibleLines);

        case SDLK_LEFTBRACKET:
            return handleCommandJumpToTop(state);

        case SDLK_RIGHTBRACKET:
            return handleCommandJumpToBottom(buffer, state, maxVisibleLines);

        case SDLK_BACKSPACE:
            return handleCommandTrimLineStart(line, state);

        default:
            return false;
    }
}






static bool handleCommandDuplicateLine(EditorView* view, EditorBuffer* buffer, EditorState* state) {
    OpenFile* file = view->openFiles[view->activeTab];
    if (!file) return false;

    commitWordEdit(file);
    pushUndoState(file);

    if (buffer->lineCount >= buffer->capacity) return false;

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
    return true;
}

static bool handleCommandPasteCutBuffer(EditorView* view, EditorBuffer* buffer, EditorState* state) {
    OpenFile* file = view->openFiles[view->activeTab];
    if (!file) return false;

    commitWordEdit(file);
    pushUndoState(file);

    if (editorCutBuffer.count == 0 ||
        buffer->lineCount + editorCutBuffer.count > buffer->capacity)
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










static bool handleCommandCopySelection(EditorBuffer* buffer, EditorState* state) {
    if (!state->selecting) return false;

    int startRow = state->selStartRow, startCol = state->selStartCol;
    int endRow = state->cursorRow, endCol = state->cursorCol;

    if (startRow > endRow || (startRow == endRow && startCol > endCol)) {
        int tmpRow = startRow, tmpCol = startCol;
        startRow = endRow; startCol = endCol;
        endRow = tmpRow;   endCol = tmpCol;
    }

    selectionBuffer[0] = '\0';
    for (int r = startRow; r <= endRow; r++) {
        const char* line = buffer->lines[r];
        int from = (r == startRow) ? startCol : 0;
        int to   = (r == endRow)   ? endCol   : strlen(line);
        strncat(selectionBuffer, line + from, to - from);
        if (r != endRow) strcat(selectionBuffer, "\n");
    }

    state->selecting = false;
    return true;
}

static bool handleCommandCutSelection(EditorBuffer* buffer, EditorState* state) {
    if (!state->selecting) return false;

    IDECoreState* core = getCoreState();
    EditorView* view = core->activeEditorView;
    if (!view || view->activeTab < 0 || view->activeTab >= view->fileCount) return false;
    OpenFile* file = view->openFiles[view->activeTab];
    if (!file) return false;

    pushUndoState(file);

    int startRow = state->selStartRow, startCol = state->selStartCol;
    int endRow = state->cursorRow, endCol = state->cursorCol;

    if (startRow > endRow || (startRow == endRow && startCol > endCol)) {
        int tmpRow = startRow, tmpCol = startCol;
        startRow = endRow; startCol = endCol;
        endRow = tmpRow;   endCol = tmpCol;
    }

    selectionBuffer[0] = '\0';
    for (int r = startRow; r <= endRow; r++) {
        const char* line = buffer->lines[r];
        int from = (r == startRow) ? startCol : 0;
        int to   = (r == endRow)   ? endCol   : strlen(line);
        strncat(selectionBuffer, line + from, to - from);
        if (r != endRow) strcat(selectionBuffer, "\n");
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

    return true;
}

static bool handleCommandPasteClipboard(EditorBuffer* buffer, EditorState* state) {
    if (strlen(selectionBuffer) == 0) return false;

    IDECoreState* core = getCoreState();
    EditorView* view = core->activeEditorView;
    if (!view || view->activeTab < 0 || view->activeTab >= view->fileCount) return false;
    OpenFile* file = view->openFiles[view->activeTab];
    if (!file) return false;

    pushUndoState(file);

    char* currentLine = buffer->lines[state->cursorRow];
    int currentLen = strlen(currentLine);
    int insertPos = state->cursorCol;

    char* beforeCursor = malloc(insertPos + 1);
    char* afterCursor  = malloc(currentLen - insertPos + 1);
    if (!beforeCursor || !afterCursor) return false;

    strncpy(beforeCursor, currentLine, insertPos); 
    beforeCursor[insertPos] = '\0';
    strcpy(afterCursor, currentLine + insertPos);

    free(buffer->lines[state->cursorRow]);

    char* selectionCopy = strdup(selectionBuffer);
    if (!selectionCopy) return false;

    char* line = strtok(selectionCopy, "\n");
    int row = state->cursorRow;

    if (line) {
        char* newLine = malloc(strlen(beforeCursor) + strlen(line) + 1);
        sprintf(newLine, "%s%s", beforeCursor, line);
        buffer->lines[row] = newLine;
    }

    while ((line = strtok(NULL, "\n")) != NULL) {
        if (buffer->lineCount < buffer->capacity) {
            for (int i = buffer->lineCount; i > row + 1; i--) {
                buffer->lines[i] = buffer->lines[i - 1];
            }
            buffer->lines[row + 1] = strdup(line);
            buffer->lineCount++;
            row++;
        }
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











void handleCommandShiftAction(SDL_Keycode key, EditorBuffer* buffer, EditorState* state) {
    if (!buffer || !state) return;

    beginTextSelectionIfNeeded(state);
    handleArrowKeyPress(key, buffer, state, 0);  // keep collapsed logic for now
}


bool isSpecialShiftAction(SDL_Keycode key) {
    switch (key) {
        // Shift+Arrow keys still go here if needed
        default:
            return false;
    }
}









static void handleCommandUndo(OpenFile* file) {
    if (performUndo(file)) {
        printf("[Undo] Performed (Alt+-).\n");
    }
}

static void handleCommandRedo(OpenFile* file) {
    if (performRedo(file)) {
        printf("[Redo] Performed (Alt+=).\n");
    }
}



void handleCommandAltAction(SDL_Keycode key, EditorBuffer* buffer, EditorState* state) {
    IDECoreState* core = getCoreState();
    EditorView* view = core->activeEditorView;

    if (!view || view->activeTab < 0 || view->activeTab >= view->fileCount) return;
    OpenFile* file = view->openFiles[view->activeTab];
    if (!file) return;

    switch (key) {
        case SDLK_MINUS:
            handleCommandUndo(file);
            break;

        case SDLK_EQUALS:
            handleCommandRedo(file);
            break;

        default:
            break;
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

    if (!view || view->type != VIEW_LEAF || view->activeTab < 0 || view->activeTab >= view->fileCount) {
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





static void handleCommandInsertNewline(OpenFile* file, EditorBuffer* buffer, EditorState* state) {
    commitWordEdit(file);
    pushUndoState(file);
    handleReturnKey(buffer, state);  // keep this as your newline logic
}

static void handleCommandDeleteCharacter(OpenFile* file, EditorBuffer* buffer, EditorState* state) {
    pushUndoState(file);
    handleBackspaceKey(buffer, state);  // keep your backspace handler here
}

static void handleCommandInsertCharacter(OpenFile* file, EditorBuffer* buffer,
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












//		KEY PRESSES
//	===============================
// 		MOUSE POSITIONS









static bool handleCommandClickOnScrollbar(int mouseX, int mouseY, SDL_Event* event) {
    for (int i = 0; i < scrollThumbHitboxCount; i++) {
        ScrollThumbHitbox* hit = &scrollThumbHitboxes[i];
        if (SDL_PointInRect(&(SDL_Point){mouseX, mouseY}, &hit->rect)) {
            return handleEditorScrollbarEvent(hit->pane, event);
        }
    }
    return false;
}

static bool handleCommandClickOnTabBar(EditorView* view, int mouseX, int mouseY) {
    for (int i = 0; i < viewTabHitboxCount; i++) {
        if (viewTabHitboxes[i].view != view) continue;

        SDL_Rect tabRect = viewTabHitboxes[i].rect;
        if (SDL_PointInRect(&(SDL_Point){mouseX, mouseY}, &tabRect)) {
            int tabIndex = viewTabHitboxes[i].tabIndex;
            if (tabIndex >= 0 && tabIndex < view->fileCount) {
                view->activeTab = tabIndex;
                setActiveEditorView(view);
                printf("[Tab] Switched to tab %d in view %p\n", tabIndex, (void*)view);
            }
            return true;
        }
    }
    return false;
}

static void getClickedEditorPosition(int mouseX, int mouseY, EditorView* view,
                                     EditorBuffer* buffer, EditorState* state,
                                     int* outRow, int* outCol) {
    const int lineHeight = 20;
    const int paddingLeft = 8;
    const int textX = view->x + paddingLeft;
    const int textY = view->y + HEADER_HEIGHT + 2 + state->verticalPadding;

    int row = state->viewTopRow + (mouseY - textY) / lineHeight;
    if (row < 0) row = 0;
    if (row >= buffer->lineCount) row = buffer->lineCount - 1;

    const char* line = buffer->lines[row];
    int col = 0;
    int accumulatedX = textX;

    while (line[col]) {
        int charWidth = getTextWidthN(line, col + 1) - getTextWidthN(line, col);
        if (accumulatedX + 4 + charWidth / 2 >= mouseX) break;
        accumulatedX += charWidth;
        col++;
    }

    *outRow = row;
    *outCol = col;
}

static void handleCommandEditorMouseClick(EditorState* state, SDL_Event* event, int row, int col) {
    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
        state->cursorRow = row;
        state->cursorCol = col;
        state->selStartRow = row;
        state->selStartCol = col;
        state->selecting = true;
        state->draggingWithMouse = true;
    } else if (event->type == SDL_MOUSEBUTTONUP) {
        state->draggingWithMouse = false;
        state->draggingOutsidePane = false;
    }
}


bool handleEditorScrollbarThumbClick(SDL_Event* event, int mx, int my) {
    if (event->type != SDL_MOUSEBUTTONDOWN || event->button.button != SDL_BUTTON_LEFT)
	 return false;
        
    SDL_Point mousePoint = { mx, my };
            
    for (int i = 0; i < scrollThumbHitboxCount; i++) {
        if (SDL_PointInRect(&mousePoint, &scrollThumbHitboxes[i].rect)) {
            UIPane* thumbPane = scrollThumbHitboxes[i].pane;
            EditorView* thumbView = scrollThumbHitboxes[i].view;
    
            if (thumbView && thumbPane && thumbPane->role == PANE_ROLE_EDITOR) {
                OpenFile* file = thumbView->openFiles[thumbView->activeTab];
                if (file && file->buffer) {
                    setActiveEditorView(thumbView);
                    beginEditorScrollbarDrag(thumbPane, thumbView);
                    file->state.scrollbarDragOffsetY = my - scrollThumbHitboxes[i].rect.y;
                    return true;
                }
            }   
        }
    }
    
    return false;
}
           



void handleEditorMouseClick(UIPane* pane, SDL_Event* event, EditorView* clickedView) {
    if (!pane || !pane->editorView || !clickedView || clickedView->type != VIEW_LEAF)
        return;

    int mouseX = event->button.x;
    int mouseY = event->button.y;

    setActiveEditorView(clickedView);

    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
        if (handleCommandClickOnScrollbar(mouseX, mouseY, event)) return;
    }

    if (handleCommandClickOnTabBar(clickedView, mouseX, mouseY)) return;

    if (clickedView->fileCount <= 0) return;

    OpenFile* file = clickedView->openFiles[clickedView->activeTab];
    if (!file || !file->buffer) return;

    EditorBuffer* buffer = file->buffer;
    EditorState* state = &file->state;

    int clickedLine, clickedCol;
    getClickedEditorPosition(mouseX, mouseY, clickedView, buffer, state, &clickedLine, &clickedCol);

    handleCommandEditorMouseClick(state, event, clickedLine, clickedCol);
}








static void adjustCursorColWithinLine(EditorBuffer* buffer, EditorState* state) {
    const char* newLine = buffer->lines[state->cursorRow];
    int newLen = newLine ? strlen(newLine) : 0;
    if (state->cursorCol > newLen)
        state->cursorCol = newLen;
}


bool handleEditorScrollbarDrag(SDL_Event* event) {
    if (!isEditorDraggingScrollbar()) return false;
    
    UIPane* draggingPane = getDraggingEditorPane();
    EditorView* draggingView = getDraggingEditorView();
    if (draggingPane && draggingView) {
        handleEditorScrollbarEvent(draggingPane, event);
        if (event->type == SDL_MOUSEBUTTONUP)
            endEditorScrollbarDrag();
    }
    return true;
}




static void handleCommandMouseDragOutside(UIPane* pane, EditorBuffer* buffer,
                                          EditorState* state, int mouseY,
                                          int paneTop, int paneBottom, bool backInside) {
    if (backInside) {
        state->draggingOutsidePane = false;
        state->draggingReturnedToPane = true;

        bool moved = resetCursorPositionToMouse(pane, state->lastMouseX, state->lastMouseY, buffer, state);
        if (moved && !state->selecting) {
            state->selecting = true;
            state->selStartRow = state->cursorRow;
            state->selStartCol = state->cursorCol;
        }
    } else {
        if (mouseY < paneTop && state->cursorRow > 0) {
            state->cursorRow--;
            adjustCursorColWithinLine(buffer, state);
        }
        if (mouseY > paneBottom && state->cursorRow < buffer->lineCount - 1) {
            state->cursorRow++;
            adjustCursorColWithinLine(buffer, state);
        }
    }
}

static void handleCommandMouseDragInPane(UIPane* pane, EditorBuffer* buffer,
                                         EditorState* state, int mouseX, int mouseY) {
    resetCursorPositionToMouse(pane, mouseX, mouseY, buffer, state);
}



void handleEditorMouseDrag(UIPane* pane, SDL_Event* event, EditorView* view) {
    if (!view || view->type != VIEW_LEAF || view->fileCount <= 0) return;

    OpenFile* file = view->openFiles[view->activeTab];
    if (!file || !file->buffer) return;

    EditorBuffer* buffer = file->buffer;
    EditorState* state = &file->state;

    state->selecting = true;

    int mouseX = event->motion.x;
    int mouseY = event->motion.y;

    state->lastMouseX = mouseX;
    state->lastMouseY = mouseY;

    int paneTop = pane->y;
    int paneBottom = pane->y + pane->h;
    bool backInside = (mouseY >= paneTop && mouseY <= paneBottom);

    if (state->draggingOutsidePane) {
        handleCommandMouseDragOutside(pane, buffer, state, mouseY, paneTop, paneBottom, backInside);
    } else {
        handleCommandMouseDragInPane(pane, buffer, state, mouseX, mouseY);
    }
}















bool resetCursorPositionToMouse(UIPane* pane, int mouseX, int mouseY,
                                EditorBuffer* buffer, EditorState* state) {
    int lineHeight = 20;
    int textX = pane->x + 8;
    int startY = pane->y + HEADER_HEIGHT + 2 + state->verticalPadding;

    int contentTop = startY;
    int contentBottom = pane->y + pane->h;

    if (mouseY < contentTop) mouseY = contentTop;
    if (mouseY > contentBottom - 1) mouseY = contentBottom - 1;

    int visibleLineIndex = (mouseY - startY) / lineHeight;
    int newRow = state->viewTopRow + visibleLineIndex;
    if (newRow < 0) newRow = 0;
    if (newRow >= buffer->lineCount) newRow = buffer->lineCount - 1;

    const char* line = buffer->lines[newRow];
    int newCol = 0;
    int accumulatedWidth = textX;

    while (line[newCol]) {
        int charWidth = getTextWidthN(line, newCol + 1) - getTextWidthN(line, newCol);
        if (accumulatedWidth + charWidth / 2 >= mouseX) break;
        accumulatedWidth += charWidth;
        newCol++;
    }

    int lineLen = strlen(line);
    if (newCol > lineLen) newCol = lineLen;

    bool moved = (newRow != state->cursorRow || newCol != state->cursorCol);

    state->cursorRow = newRow;
    state->cursorCol = newCol;

    return moved;
}




// 		Event Handling

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









//		Event Handling
// ============================================
// 		API LAYER



void insertCharAtCursor(char ch) {
    IDECoreState* core = getCoreState();
    EditorView* view = core->activeEditorView;
    if (!view || view->activeTab < 0) return;

    OpenFile* file = view->openFiles[view->activeTab];
    if (!file || !file->buffer) return;

    handleCommandInsertCharacter(file, file->buffer, &file->state, ch);
}

void deleteCharAtCursor(void) {
    IDECoreState* core = getCoreState();
    EditorView* view = core->activeEditorView;
    if (!view || view->activeTab < 0) return;

    OpenFile* file = view->openFiles[view->activeTab];
    if (!file || !file->buffer) return;

    handleCommandDeleteCharacter(file, file->buffer, &file->state);
}

void cutSelection(EditorView* view) {
    if (!view || view->activeTab < 0) return;
    OpenFile* file = view->openFiles[view->activeTab];
    if (!file || !file->buffer) return;

    handleCommandCutSelection(file->buffer, &file->state);
}

void copySelection(EditorView* view) {
    if (!view || view->activeTab < 0) return;
    OpenFile* file = view->openFiles[view->activeTab];
    if (!file || !file->buffer) return;

    handleCommandCopySelection(file->buffer, &file->state);
}

void pasteClipboard(EditorView* view) {
    if (!view || view->activeTab < 0) return;
    OpenFile* file = view->openFiles[view->activeTab];
    if (!file || !file->buffer) return;

    handleCommandPasteClipboard(file->buffer, &file->state);
}

void selectAllText(EditorView* view) {
    if (!view || view->activeTab < 0) return;
    OpenFile* file = view->openFiles[view->activeTab];
    if (!file || !file->buffer) return;

    EditorState* state = &file->state;
    state->selStartRow = 0;
    state->selStartCol = 0;
    state->cursorRow = file->buffer->lineCount - 1;
    state->cursorCol = strlen(file->buffer->lines[state->cursorRow]);
    state->selecting = true;
}




//              API LAYER

