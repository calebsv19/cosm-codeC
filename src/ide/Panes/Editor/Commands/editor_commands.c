#include "ide/Panes/Editor/Commands/editor_commands.h"
#include "ide/Panes/Editor/editor_clipboard.h"
#include "ide/Panes/Editor/editor_core.h"
#include "app/GlobalInfo/project.h"

#include <string.h>




static void handleCommandAddEditorView(UIPane* pane, SDL_Keymod mod) {
    IDECoreState* core = getCoreState();
    if (!core || !core->editorPane) return;

    EditorView* root = core->editorPane->editorView;
    EditorView* target = core->activeEditorView;
    SplitOrientation split = (mod & KMOD_SHIFT) ? SPLIT_HORIZONTAL : SPLIT_VERTICAL;

    printf("[COMMAND] Split editor view requested (%s).\n",
           split == SPLIT_HORIZONTAL ? "horizontal" : "vertical");
    if (!splitEditorView(root, target, pane, split)) {
        printf("[COMMAND] Split request ignored (no valid target or max split limit).\n");
    }
}
     
static void handleCommandSwitchTabWithMod(SDL_Keymod mod) {
    int direction = (mod & KMOD_SHIFT) ? -1 : 1;
    switchTab(getCoreState()->activeEditorView, direction);
}
 
void handleCommandAction(SDL_Keycode key, EditorBuffer* buffer, EditorState* state,
                         EditorView* view, UIPane* pane, SDL_Keymod mod) {
    (void)buffer;
    (void)state;
    (void)view;
    if (key == SDLK_e) {
        handleCommandAddEditorView(pane, mod);
    }
    
    if (key == SDLK_TAB) {
        handleCommandSwitchTabWithMod(mod);
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

    editorStateSetTopRow(state, state->cursorRow - maxVisibleLines / 2);
    
    return true;
}

static bool handleCommandJumpToTop(EditorState* state) {
    state->cursorRow = 0;
    state->cursorCol = 0;
    editorStateSetTopRow(state, 0);
    return true;
}
 
static bool handleCommandJumpToBottom(EditorBuffer* buffer, EditorState* state, int maxVisibleLines) {
    state->cursorRow = buffer->lineCount - 1;
    state->cursorCol = strlen(buffer->lines[state->cursorRow]);
    editorStateSetTopRow(state, state->cursorRow - maxVisibleLines + 1);
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
    int lineHeight = EDITOR_LINE_HEIGHT;
    int maxVisibleLines = (paneHeight - EDITOR_CONTENT_TOP_PADDING) / lineHeight;
 
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

bool editor_jump_to(EditorView* view, const char* filePath, int line, int column) {
    if (!view) return false;

    OpenFile* file = NULL;
    if (filePath && filePath[0]) {
        char fullPath[1024];
        if (filePath[0] == '/') {
            snprintf(fullPath, sizeof(fullPath), "%s", filePath);
        } else {
            snprintf(fullPath, sizeof(fullPath), "%s/%s", projectPath, filePath);
        }
        file = openFileInView(view, fullPath);
    } else {
        file = getActiveOpenFile(view);
    }

    if (!file || !file->buffer) return false;

    int targetRow = line > 0 ? line - 1 : 0;
    if (targetRow >= file->buffer->lineCount) targetRow = file->buffer->lineCount - 1;
    if (targetRow < 0) targetRow = 0;
    int lineLen = (file->buffer->lines && file->buffer->lines[targetRow])
                      ? (int)strlen(file->buffer->lines[targetRow])
                      : 0;
    int targetCol = column > 0 ? column - 1 : 0;
    if (targetCol > lineLen) targetCol = lineLen;

    file->state.cursorRow = targetRow;
    file->state.cursorCol = targetCol;
    editorStateSetTopRow(&file->state, (targetRow > 2) ? targetRow - 2 : 0);
    file->state.selecting = false;
    file->state.draggingWithMouse = false;
    setActiveEditorView(view);
    return true;
}



// =========================================
// 		EXTERNAL REFERENCE


void handleCommandMovement(SDL_Keycode key, EditorBuffer* buffer, EditorState* state,
                           int paneHeight, SDL_Keymod mod) {
    
    if (!buffer || buffer->lineCount == 0) return;

    bool shiftHeld = mod & KMOD_SHIFT;
 
    if (handleCommandNavigation(key, buffer, state, paneHeight, shiftHeld)) return;
    if (handleCommandLineClipboard(key, buffer, state)) return;
    if (handleCommandTextClipboard(key, buffer, state)) return;
    
}
