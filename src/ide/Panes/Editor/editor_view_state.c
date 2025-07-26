#include "editor_view_state.h"
#include "editor_view.h"
#include "ide/Panes/PaneInfo/pane.h"
#include "app/GlobalInfo/core_state.h"


// === Dragging State Pointers ===
void setDraggingEditorPane(UIPane* pane) {
    getCoreState()->editorViewState->draggingPane = pane;
}

void setDraggingEditorView(EditorView* view) {
    getCoreState()->editorViewState->draggingView = view;
}

EditorView* getDraggingEditorView() {
    return getCoreState()->editorViewState->draggingView;
}

UIPane* getDraggingEditorPane() {
    return getCoreState()->editorViewState->draggingPane;
}

bool isEditorDraggingScrollbar() {
    return getCoreState()->editorViewState->draggingPane != NULL;
}

void beginEditorScrollbarDrag(UIPane* pane, EditorView* view) {
    EditorViewState* state = getCoreState()->editorViewState;

    state->draggingPane = pane;
    state->draggingView = view;
    state->draggingScrollbar = true;

    if (!view || view->activeTab < 0 || view->activeTab >= view->fileCount) return;
    OpenFile* file = view->openFiles[view->activeTab];
    if (!file) return;

    file->state.scrollbarHasMovedYet = false;
}

void endEditorScrollbarDrag() {
    EditorViewState* state = getCoreState()->editorViewState;
    state->draggingPane = NULL;
    state->draggingView = NULL;
    state->draggingScrollbar = false;
}


// === Hitbox Collection & Layout ===

void resetViewCounters(void) {
    EditorViewState* state = getCoreState()->editorViewState;
    state->viewTabHitboxCount = 0;
    state->leafHitboxCount = 0;
    state->scrollThumbHitboxCount = 0;
}

void resetLeafHitboxesOnly(void) {
    EditorViewState* state = getCoreState()->editorViewState;
    state->leafHitboxCount = 0;
}


void addScrollThumbHitbox(SDL_Rect rect, EditorView* view, UIPane* pane) {
    EditorViewState* state = getCoreState()->editorViewState;
    if (state->scrollThumbHitboxCount >= MAX_SCROLL_HITBOXES) return;

    ScrollThumbHitbox* hitbox = &state->scrollThumbHitboxes[state->scrollThumbHitboxCount++];
    hitbox->rect = rect;
    hitbox->view = view;
    hitbox->pane = pane;
}

void addLeafHitbox(SDL_Rect rect, EditorView* view) {
    EditorViewState* state = getCoreState()->editorViewState;
    if (state->leafHitboxCount >= MAX_LEAF_HITBOXES) return;

    LeafHitbox* hitbox = &state->leafHitboxes[state->leafHitboxCount++];
    hitbox->rect = rect;
    hitbox->view = view;
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
    resetViewCounters();             // clears global viewState counts
    collectLeafHitboxes(root);       // repopulates them based on layout
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
