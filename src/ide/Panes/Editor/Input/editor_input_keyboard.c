#include "editor_input_keyboard.h"
#include "app/GlobalInfo/core_state.h"
#include "ide/Panes/Editor/editor.h"
#include "ide/Panes/Editor/undo_stack.h"
#include "ide/Panes/Editor/Commands/editor_commands.h"
#include "ide/Panes/Editor/editor_text_edit.h"
#include "ide/Panes/Editor/editor_view.h"

#include <string.h>


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
    
    

static void handleCommandAltAction(SDL_Keycode key, EditorView* view) {
    if (!view || view->type != VIEW_LEAF ||
        view->activeTab < 0 || view->activeTab >= view->fileCount) return;

    OpenFile* file = getActiveOpenFile(view);
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

static bool editor_keyboard_projection_mode_active(const OpenFile* file) {
    return file &&
           editor_file_projection_active(file) &&
           file->projection.lines &&
           file->projection.lineCount > 0;
}

static void handleCommandSwitchTab(int direction) {
    EditorView* view = getCoreState()->activeEditorView;
    if (view) switchTab(view, direction);
}

static void handleCommandMoveCursor(SDL_Keycode key, EditorBuffer* buffer,
                                    EditorState* state, int paneHeight) {
    handleArrowKeyPress(key, buffer, state, paneHeight);
}


void editorProcessKeyCommand(UIPane* pane, EditorKeyCommandPayload* payload) {
    if (!payload) return;

    IDECoreState* core = getCoreState();
    EditorView* view = core ? core->activeEditorView : NULL;
    SDL_Keycode key = payload->key;
    SDL_Keymod mod = payload->mod;

    bool shiftHeld = (mod & KMOD_SHIFT) != 0;
    bool cmdHeld   = (mod & KMOD_GUI) != 0;
    bool altHeld   = (mod & KMOD_ALT) != 0;

    if (cmdHeld && key == SDLK_e) {
        handleCommandAction(key, NULL, NULL, view, pane, mod);
        return;
    }
    if (altHeld && key == SDLK_c) {
        if (core && core->persistentEditorView && view && view->type == VIEW_LEAF) {
            (void)collapseEditorLeaf(core->persistentEditorView, view);
        }
        return;
    }

    if (!view || view->type != VIEW_LEAF || view->fileCount <= 0) return;

    OpenFile* file = getActiveOpenFile(view);
    if (!file || !file->buffer) return;

    EditorBuffer* buffer = file->buffer;
    EditorState* state = &file->state;
    bool projectionMode = editor_keyboard_projection_mode_active(file);

    if ((shiftHeld || cmdHeld) &&
        (key == SDLK_LEFT || key == SDLK_RIGHT || key == SDLK_UP || key == SDLK_DOWN)) {
        beginTextSelectionIfNeeded(state);
    }

    int paneHeight = pane ? pane->h : 0;

    if (isArrowKey(key) || key == SDLK_HOME || key == SDLK_END) {
        handleCommandMoveCursor(key, buffer, state, paneHeight);
        if (!shiftHeld) state->selecting = false;
        return;
    }

    if (cmdHeld) {
        if (key == SDLK_1) {
            handleCommandSwitchTab(+1);
            return;
        }
        if (projectionMode) return;
        handleCommandMovement(key, buffer, state, paneHeight, mod);
        handleCommandAction(key, buffer, state, view, pane, mod);
        return;
    }

    if (shiftHeld && isSpecialShiftAction(key)) {
        handleCommandShiftAction(key, buffer, state);
        return;
    }

    if (altHeld) {
        if (projectionMode) return;
        handleCommandAltAction(key, view);
        return;
    }
}


void editorProcessTextInput(UIPane* pane, const EditorTextInputPayload* payload) {
    (void)pane;
    if (!payload) return;

    IDECoreState* core = getCoreState();
    EditorView* view = core ? core->activeEditorView : NULL;
    if (!view || view->type != VIEW_LEAF || view->fileCount <= 0) return;
    OpenFile* active = getActiveOpenFile(view);
    if (editor_keyboard_projection_mode_active(active)) return;

    const char* text = payload->text;
    for (int i = 0; text[i] != '\0'; ++i) {
        insertCharAtCursor(text[i]);
    }
}




//              TEXT EDITS
//====================================================
