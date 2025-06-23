#include "Editor/editor_core.h"

#include "Editor/editor_view.h"  // for EditorView and OpenFile
#include "Editor/editor_state.h"
#include "Render/render_text_helpers.h"
#include "Editor/undo_stack.h"
    
#include "CommandBus/command_bus.h"
#include "GlobalInfo/core_state.h"
#include "PaneInfo/pane.h"
#include "GlobalInfo/system_control.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>





//              INITS
// ============================================
// 		helpers




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
