#include "ide/Panes/Editor/editor_state.h"
#include "ide/Panes/Editor/editor_view.h"
#include "ide/Panes/Editor/editor_view_state.h"
#include "ide/Panes/PaneInfo/pane.h"
#include <stdlib.h>
#include <string.h>

// ====== Scrollbar Dragging State Management ======

// static EditorView* draggingEditorView = NULL;
// static UIPane* draggingEditorPane = NULL;




// ====== Lifecycle ======

EditorState* createEditorState(void) {
    EditorState* state = (EditorState*)malloc(sizeof(EditorState));
    if (!state) return NULL;
    resetEditorState(state);
    return state;
}

void setEditorVerticalPaddingIfUnset(EditorState* state, int padding) {
    if (state && state->verticalPadding == 0) {
        state->verticalPadding = padding;
    }
}


void resetEditorState(EditorState* state) {
    if (!state) return;
    state->cursorRow = 0;
    state->cursorCol = 0;
    state->viewTopRow = 0;
    state->verticalPadding = 80;

    state->lastMouseX = 0;
    state->lastMouseY = 0;
    state->selecting = false;
    state->selStartRow = 0;
    state->selStartCol = 0;

    state->mouseHasMovedSinceClick = false;
    state->draggingWithMouse = false;
    state->draggingOutsidePane = false;
    state->draggingReturnedToPane = false;
    state->scrollbarStartY = 0;
    state->initialTopRow = 0;
    state->scrollbarDragOffsetY = 0;
    state->scrollbarHasMovedYet = false;

    state->closeButtonRect = (SDL_Rect){0, 0, 0, 0};
}

// Free memory
void freeEditorState(EditorState* state) {
    if (state) free(state);
}

// ====== Utilities ======

void startSelection(EditorState* state) {
    if (!state) return;
    state->selecting = true;
    state->selStartRow = state->cursorRow;
    state->selStartCol = state->cursorCol;
}

void stopSelection(EditorState* state) {
    if (!state) return;
    state->selecting = false;
}

bool isDragging(EditorState* state) {
    return state && (state->draggingWithMouse || isEditorDraggingScrollbar());
}




