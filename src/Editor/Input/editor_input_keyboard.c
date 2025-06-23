#include "editor_input_keyboard.h"
#include "Editor/undo_stack.h"
#include "Editor/Commands/editor_commands.h"
#include "Editor/editor_text_edit.h"


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
//==============================================
//		Alt Action



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




//              ALT ACTION
//==============================================
//              SHIFT ACTION

static void beginTextSelectionIfNeeded(EditorState* state) {
    if (!state->selecting) {
        state->selecting = true;
        state->selStartRow = state->cursorRow;
        state->selStartCol = state->cursorCol;
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





//              SHIFT ACTION  
//  ==============================================
//		MAIN KEYDOWN


static bool isArrowKey(SDL_Keycode key) {
    return key == SDLK_LEFT || key == SDLK_RIGHT ||
           key == SDLK_UP || key == SDLK_DOWN;
}

static void handleCommandSwitchTab(int direction) {
    EditorView* view = getCoreState()->activeEditorView;
    if (view) switchTab(view, direction);
}

static void handleCommandMoveCursor(SDL_Keycode key, EditorBuffer* buffer,
					EditorState* state, int paneHeight) {
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




//              TEXT EDITS
//====================================================




