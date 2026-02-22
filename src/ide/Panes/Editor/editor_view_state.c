#include "editor_view_state.h"
#include "editor_view.h"
#include "ide/Panes/PaneInfo/pane.h"
#include "app/GlobalInfo/core_state.h"

#define SPLIT_DIVIDER_HIT_THICKNESS 12

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

bool isEditorDraggingSplitDivider(void) {
    return getCoreState()->editorViewState->draggingSplitDivider;
}

EditorView* getDraggingSplitEditorView(void) {
    return getCoreState()->editorViewState->draggingSplit;
}

void beginEditorSplitDividerDrag(EditorView* splitView, int mouseX, int mouseY) {
    EditorViewState* state = getCoreState()->editorViewState;
    if (!splitView || splitView->type != VIEW_SPLIT) return;
    state->draggingSplitDivider = true;
    state->draggingSplit = splitView;
    state->splitDragStartMouseX = mouseX;
    state->splitDragStartMouseY = mouseY;
    state->splitDragStartRatio = splitView->splitRatio;
}

void endEditorSplitDividerDrag(void) {
    EditorViewState* state = getCoreState()->editorViewState;
    state->draggingSplitDivider = false;
    state->draggingSplit = NULL;
    state->splitDragStartMouseX = 0;
    state->splitDragStartMouseY = 0;
    state->splitDragStartRatio = 0.0f;
}


// === Hitbox Collection & Layout ===

void resetViewCounters(void) {
    EditorViewState* state = getCoreState()->editorViewState;
    state->viewTabHitboxCount = 0;
    state->leafHitboxCount = 0;
    state->scrollThumbHitboxCount = 0;
    state->splitDividerHitboxCount = 0;
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

void addSplitDividerHitboxForView(EditorView* splitView) {
    EditorViewState* state = getCoreState()->editorViewState;
    if (!splitView || splitView->type != VIEW_SPLIT || !splitView->childA || !splitView->childB) return;
    if (state->splitDividerHitboxCount >= MAX_SPLIT_HITBOXES) return;

    SDL_Rect rect = {0};
    if (splitView->splitType == SPLIT_VERTICAL) {
        int gapStart = splitView->childA->x + splitView->childA->w;
        int gapEnd = splitView->childB->x;
        int centerX = (gapStart + gapEnd) / 2;
        rect.x = centerX - (SPLIT_DIVIDER_HIT_THICKNESS / 2);
        rect.y = splitView->y;
        rect.w = SPLIT_DIVIDER_HIT_THICKNESS;
        rect.h = splitView->h;
    } else {
        int gapStart = splitView->childA->y + splitView->childA->h;
        int gapEnd = splitView->childB->y;
        int centerY = (gapStart + gapEnd) / 2;
        rect.x = splitView->x;
        rect.y = centerY - (SPLIT_DIVIDER_HIT_THICKNESS / 2);
        rect.w = splitView->w;
        rect.h = SPLIT_DIVIDER_HIT_THICKNESS;
    }

    if (rect.w <= 0 || rect.h <= 0) return;
    SplitDividerHitbox* hitbox = &state->splitDividerHitboxes[state->splitDividerHitboxCount++];
    hitbox->rect = rect;
    hitbox->splitView = splitView;
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

EditorView* hitTestLeaf(EditorViewState* state, int mouseX, int mouseY) {
    if (!state) return NULL;

    for (int i = 0; i < state->leafHitboxCount; i++) {
        LeafHitbox* hit = &state->leafHitboxes[i];
        SDL_Rect rect = hit->rect;

        if (mouseX >= rect.x && mouseX < rect.x + rect.w &&
            mouseY >= rect.y && mouseY < rect.y + rect.h) {
            return (EditorView*)hit->view;
        }
    }

    return NULL;
}

EditorView* hitTestSplitDivider(EditorViewState* state, int mouseX, int mouseY) {
    if (!state) return NULL;
    SDL_Point p = { mouseX, mouseY };

    for (int i = state->splitDividerHitboxCount - 1; i >= 0; --i) {
        SplitDividerHitbox* hit = &state->splitDividerHitboxes[i];
        if (SDL_PointInRect(&p, &hit->rect)) {
            return (EditorView*)hit->splitView;
        }
    }
    return NULL;
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
