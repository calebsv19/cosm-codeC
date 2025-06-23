#include "Editor/Commands/editor_commands.h"
#include "Editor/editor_clipboard.h"
#include "Editor/editor_core.h"




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
    




//		ACTION
// ==================================
// 		NAVIGATION

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
 
static bool handleCommandSkipParagraph(EditorBuffer* buffer, EditorState* state, int direction, int 
maxVisibleLines) {
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



// =========================================
// 		EXTERNAL REFERENCE


void handleCommandMovement(SDL_Keycode key, EditorBuffer* buffer, EditorState* state,
                                EditorView* view, int paneHeight) {
    
    if (!buffer || buffer->lineCount == 0) return;
     
    SDL_Keymod mod = SDL_GetModState();
    bool shiftHeld = mod & KMOD_SHIFT;
 
    if (handleCommandNavigation(key, buffer, state, paneHeight, shiftHeld)) return;
    if (handleCommandLineClipboard(key, buffer, state)) return;
    if (handleCommandTextClipboard(key, buffer, state)) return;
    
}
