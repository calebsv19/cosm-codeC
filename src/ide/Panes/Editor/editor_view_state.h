#ifndef EDITOR_VIEW_STATE_H
#define EDITOR_VIEW_STATE_H

#include <SDL2/SDL.h>
#include <stdbool.h>

struct UIPane;
struct EditorView;

// === Scrollbar + View Hitbox Tracking ===
#define SCROLLBAR_WIDTH 6
#define SCROLLBAR_PADDING 2

#define MAX_TAB_HITBOXES 64
#define MAX_LEAF_HITBOXES 64
#define MAX_SCROLL_HITBOXES 32
#define MAX_SPLIT_HITBOXES 64

typedef struct {
    SDL_Rect rect;
    void* view;
    struct UIPane* pane;
} ScrollThumbHitbox;

typedef struct {
    SDL_Rect rect;
    int tabIndex;
    void* view;
} TabHitbox;

typedef struct {
    SDL_Rect rect;
    void* view;
} LeafHitbox;

typedef struct {
    SDL_Rect rect;
    void* splitView;
} SplitDividerHitbox;

typedef struct EditorViewState {
    bool draggingScrollbar;
    struct UIPane* draggingPane;
    struct EditorView* draggingView;

    ScrollThumbHitbox scrollThumbHitboxes[MAX_SCROLL_HITBOXES];
    int scrollThumbHitboxCount;

    TabHitbox viewTabHitboxes[MAX_TAB_HITBOXES];
    int viewTabHitboxCount;

    LeafHitbox leafHitboxes[MAX_LEAF_HITBOXES];
    int leafHitboxCount;

    SplitDividerHitbox splitDividerHitboxes[MAX_SPLIT_HITBOXES];
    int splitDividerHitboxCount;

    bool draggingSplitDivider;
    struct EditorView* draggingSplit;
    int splitDragStartMouseX;
    int splitDragStartMouseY;
    float splitDragStartRatio;
} EditorViewState;


// === View Hitbox + Drag Tracking ===
void resetViewCounters(void);
void resetLeafHitboxesOnly(void);
void addScrollThumbHitbox(SDL_Rect rect, struct EditorView* view, struct UIPane* pane);
void addLeafHitbox(SDL_Rect rect, struct EditorView* view);
void addSplitDividerHitboxForView(struct EditorView* splitView);
void collectLeafHitboxes(struct EditorView* view);
void rebuildLeafHitboxes(struct EditorView* root);
struct EditorView* hitTestLeaf(struct EditorViewState* state, int mouseX, int mouseY);
struct EditorView* hitTestSplitDivider(struct EditorViewState* state, int mouseX, int mouseY);

// === View Dragging State ===
struct EditorView* getDraggingEditorView(void);
struct UIPane* getDraggingEditorPane(void);
void setDraggingEditorView(struct EditorView* view);
void setDraggingEditorPane(struct UIPane* pane);

bool isEditorDraggingScrollbar(void);
void beginEditorScrollbarDrag(struct UIPane* pane, struct EditorView* view);
void endEditorScrollbarDrag(void);

bool isEditorDraggingSplitDivider(void);
struct EditorView* getDraggingSplitEditorView(void);
void beginEditorSplitDividerDrag(struct EditorView* splitView, int mouseX, int mouseY);
void endEditorSplitDividerDrag(void);

#endif // EDITOR_VIEW_STATE_H
