#include "editor_state.h"
#include "editor_view.h"
#include "pane.h"
#include <stdlib.h>
#include <string.h>

// ====== Scrollbar Dragging State Management ======

static EditorView* draggingEditorView = NULL;
static UIPane* draggingEditorPane = NULL;


bool isEditorDraggingScrollbar() {
    return draggingEditorPane != NULL;
}

// Call this when scrollbar drag starts
void beginEditorScrollbarDrag(UIPane* pane, EditorView* view) {
    setDraggingEditorPane(pane);
    setDraggingEditorView(view);

    OpenFile* file = view->openFiles[view->activeTab];
    if (!file) return;

    EditorState* state = &file->state;
    state->scrollbarHasMovedYet = false;
}


// Call this when mouse button is released
void endEditorScrollbarDrag() {
    draggingEditorPane = NULL;
    draggingEditorView = NULL;

}

void setDraggingEditorPane(UIPane* pane){
	draggingEditorPane = pane;
}

void setDraggingEditorView(EditorView* view){
        draggingEditorView = view;
}

EditorView* getDraggingEditorView() {
    return draggingEditorView;
}

UIPane* getDraggingEditorPane() {
    return draggingEditorPane;
}



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
    state->draggingWithMouse = false;
    state->draggingOutsidePane = false;
    state->draggingReturnedToPane = false;
    state->scrollbarStartY = 0;
    state->initialTopRow = 0;
    state->scrollbarDragOffsetY = 0;
    state->scrollbarHasMovedYet = false;
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


// ====== View Hitbox Tracking ======


ScrollThumbHitbox scrollThumbHitboxes[MAX_SCROLL_HITBOXES];
int scrollThumbHitboxCount = 0;

TabHitbox viewTabHitboxes[MAX_TAB_HITBOXES];
int viewTabHitboxCount = 0;

LeafHitbox leafHitboxes[MAX_LEAF_HITBOXES];
int leafHitboxCount = 0;

void resetViewCounters(void) {
    viewTabHitboxCount = 0;
    leafHitboxCount = 0;
    scrollThumbHitboxCount = 0;
}



void addScrollThumbHitbox(SDL_Rect rect, EditorView* view, UIPane* pane) {
    if (scrollThumbHitboxCount < MAX_SCROLL_HITBOXES) {
        scrollThumbHitboxes[scrollThumbHitboxCount].rect = rect;
        scrollThumbHitboxes[scrollThumbHitboxCount].view = view;
        scrollThumbHitboxes[scrollThumbHitboxCount].pane = pane;
        scrollThumbHitboxCount++;
    }
}





void addLeafHitbox(SDL_Rect rect, EditorView* view) {
    if (leafHitboxCount >= MAX_LEAF_HITBOXES) return;
    leafHitboxes[leafHitboxCount].rect = rect;
    leafHitboxes[leafHitboxCount].view = view;
    leafHitboxCount++;
}
    
void collectLeafHitboxes(EditorView* view) {
    if (!view) return;
    
    if (view->type == VIEW_LEAF) {
        SDL_Rect rect = { view->x, view->y, view->w, view->h };
        addLeafHitbox(rect, view);
        return;
    }
    
    if (view->childA) collectLeafHitboxes(view->childA);
    if (view->childB) collectLeafHitboxes(view->childB);
}   
        
void rebuildLeafHitboxes(EditorView* root) {
    resetViewCounters();             // resets all hitbox counts
    collectLeafHitboxes(root);
}
        
    
    
    
EditorView* findLeafUnderCursor(EditorView* root, int mouseX, int mouseY) {
    if (!root) return NULL;
        
    if (root->type == VIEW_LEAF) {
        if (mouseX >= root->x && mouseX < root->x + root->w &&
            mouseY >= root->y && mouseY < root->y + root->h) {
            return root;
        }
        return NULL;
    }
        
    EditorView* a = findLeafUnderCursor(root->childA, mouseX, mouseY);
    if (a) return a;
    return findLeafUnderCursor(root->childB, mouseX, mouseY);
}
