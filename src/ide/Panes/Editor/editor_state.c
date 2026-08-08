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
    if (state && state->verticalPadding <= 0) {
        state->verticalPadding = padding;
    }
}

void editorStateSetTopRow(EditorState* state, int topRow) {
    if (!state) return;
    if (topRow < 0) topRow = 0;
    state->viewTopRow = topRow;
    state->scrollOffsetPx = (float)topRow * (float)EDITOR_LINE_HEIGHT;
    state->scrollTargetPx = state->scrollOffsetPx;
}

void editorStateFrameLineInUpperBand(EditorState* state,
                                     int targetRow,
                                     int viewportHeight,
                                     int totalLines) {
    if (!state) return;
    if (targetRow < 0) targetRow = 0;
    if (totalLines > 0 && targetRow >= totalLines) targetRow = totalLines - 1;
    if (targetRow < 0) targetRow = 0;

    if (viewportHeight <= 0 || totalLines <= 0) {
        editorStateSetTopRow(state, (targetRow > 4) ? targetRow - 4 : 0);
        return;
    }

    const int lineHeight = EDITOR_LINE_HEIGHT;
    const float anchorPx = (float)viewportHeight * 0.25f;
    float targetOffset = (float)editor_vertical_padding_px(state) +
                         ((float)targetRow * (float)lineHeight) -
                         anchorPx;
    if (targetOffset < 0.0f) targetOffset = 0.0f;

    float maxOffset = editor_max_scroll_offset_px(state, totalLines, viewportHeight);
    if (targetOffset > maxOffset) targetOffset = maxOffset;

    state->scrollOffsetPx = targetOffset;
    state->scrollTargetPx = targetOffset;
    state->viewTopRow = editor_first_visible_row(state);
}

void editorStateTriggerActiveLineFlash(EditorState* state,
                                       uint64_t now_ns,
                                       uint32_t duration_ms) {
    if (!state || duration_ms == 0u) return;
    state->activeLineFlashUntilNs = now_ns + ((uint64_t)duration_ms * 1000000ULL);
}


void resetEditorState(EditorState* state) {
    if (!state) return;
    state->cursorRow = 0;
    state->cursorCol = 0;
    state->lastScrollAnchorCursorRow = -1;
    state->lastScrollAnchorCursorCol = -1;
    state->viewTopRow = 0;
    state->scrollOffsetPx = 0.0f;
    state->scrollTargetPx = 0.0f;
    state->verticalPadding = EDITOR_CONTENT_TOP_PADDING;
    state->activeLineFlashUntilNs = 0;

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
