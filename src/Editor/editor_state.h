#ifndef EDITOR_STATE_H
#define EDITOR_STATE_H

#include <stdbool.h>
#include <SDL2/SDL.h>


struct UIPane;

struct EditorView;


// Track cursor, selection, and scroll info
typedef struct {
    int cursorRow;
    int cursorCol;
    int viewTopRow;
    int verticalPadding;


    int lastMouseX;
    int lastMouseY;

    bool selecting;
    int selStartRow;
    int selStartCol;

    bool scrollbarHasMovedYet;

    bool draggingWithMouse;
    bool draggingOutsidePane;
    bool draggingReturnedToPane;

    int scrollbarStartY;
    int initialTopRow;

    int scrollbarDragOffsetY;  
} EditorState;

// Lifecycle
EditorState* createEditorState(void);
void setEditorVerticalPaddingIfUnset(EditorState* state, int padding);
void resetEditorState(EditorState* state);
void freeEditorState(EditorState* state);



// Selection utils
void startSelection(EditorState* state);
void stopSelection(EditorState* state);
bool isDragging(EditorState* state);



bool isEditorDraggingScrollbar(void);
void beginEditorScrollbarDrag(struct UIPane* pane, struct EditorView* view);
void endEditorScrollbarDrag(void);
struct EditorView* getDraggingEditorView(void);
struct UIPane* getDraggingEditorPane(void);
void setDraggingEditorView(struct EditorView* view);
void setDraggingEditorPane(struct UIPane* pane);


#define SCROLLBAR_WIDTH 6
#define SCROLLBAR_PADDING 2


// Tab and view click tracking
#define MAX_TAB_HITBOXES 64
#define MAX_LEAF_HITBOXES 64
#define MAX_SCROLL_HITBOXES 32

typedef struct {
    SDL_Rect rect;
    void* view;  // Associated EditorView* (still void* to avoid header dependency)
    struct UIPane* pane;  // Pane needed for direct routing
} ScrollThumbHitbox;

typedef struct {
    SDL_Rect rect;
    int tabIndex;
    void* view;  // void* to avoid dependency on editor_view.h
} TabHitbox;

typedef struct {
    SDL_Rect rect;
    void* view;
} LeafHitbox;

extern ScrollThumbHitbox scrollThumbHitboxes[MAX_SCROLL_HITBOXES];
extern int scrollThumbHitboxCount;


extern TabHitbox viewTabHitboxes[MAX_TAB_HITBOXES];
extern int viewTabHitboxCount;

extern LeafHitbox leafHitboxes[MAX_LEAF_HITBOXES];
extern int leafHitboxCount;

void resetViewCounters(void);
void addScrollThumbHitbox(SDL_Rect rect, struct EditorView* view, struct UIPane* pane);

void addLeafHitbox(SDL_Rect rect, struct EditorView* view);
void collectLeafHitboxes(struct EditorView* view);
void rebuildLeafHitboxes(struct EditorView* root);

#endif // EDITOR_STATE_H

