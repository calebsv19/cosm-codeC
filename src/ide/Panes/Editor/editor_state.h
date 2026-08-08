#ifndef EDITOR_STATE_H
#define EDITOR_STATE_H

#include <stdbool.h>
#include <stdint.h>
#include <SDL2/SDL.h>



// Track cursor, selection, and scroll info
typedef struct {
    int cursorRow;
    int cursorCol;
    int lastScrollAnchorCursorRow;
    int lastScrollAnchorCursorCol;
    int viewTopRow;
    float scrollOffsetPx;
    float scrollTargetPx;
    int verticalPadding;
    uint64_t activeLineFlashUntilNs;


    int lastMouseX;
    int lastMouseY;

    bool selecting;
    int selStartRow;
    int selStartCol;

    bool scrollbarHasMovedYet;

    bool mouseHasMovedSinceClick;
    bool draggingWithMouse;
    bool draggingOutsidePane;
    bool draggingReturnedToPane;

    int scrollbarStartY;
    int initialTopRow;

    int scrollbarDragOffsetY;  

    SDL_Rect closeButtonRect;
} EditorState;

// Lifecycle
EditorState* createEditorState(void);
void setEditorVerticalPaddingIfUnset(EditorState* state, int padding);
void editorStateSetTopRow(EditorState* state, int topRow);
void editorStateFrameLineInUpperBand(EditorState* state,
                                     int targetRow,
                                     int viewportHeight,
                                     int totalLines);
void editorStateTriggerActiveLineFlash(EditorState* state,
                                       uint64_t now_ns,
                                       uint32_t duration_ms);
void resetEditorState(EditorState* state);
void freeEditorState(EditorState* state);



// Selection utils
void startSelection(EditorState* state);
void stopSelection(EditorState* state);
bool isDragging(EditorState* state);


#endif // EDITOR_STATE_H
