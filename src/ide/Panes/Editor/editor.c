#include "editor.h"


#include "app/GlobalInfo/core_state.h"
#include "ide/Panes/Editor/editor_core.h"
#include "ide/Panes/Editor/editor_clipboard.h"
#include "ide/Panes/Editor/editor_text_edit.h"


CutBuffer editorCutBuffer = {
    .cutLines = {NULL},
    .count = 0
};

char selectionBuffer[MAX_SELECTION_LENGTH] = {0};


EditorBuffer* editorBuffer = NULL; // GLOBAL storage
EditorState* editorState = NULL;

//		INITS
//	-----------------------------------
//		API METHODS



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
