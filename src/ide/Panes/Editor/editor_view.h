#ifndef EDITOR_VIEW_H
#define EDITOR_VIEW_H

#include <stdint.h>
#include "app/GlobalInfo/core_state.h"
#include "editor.h"  // For EditorBuffer and EditorState
#include "ide/Panes/Editor/editor_state.h"
#include "ide/Panes/Editor/editor_view_state.h"
#include "ide/Panes/Editor/editor_buffer.h"

#include "ide/Panes/Editor/Input/input_editor.h" // for &editorInputHandler


// Shared UI layout constants
#define EDITOR_PADDING 6
#define HEADER_HEIGHT 22
#define HEADER_BG_R 40
#define HEADER_BG_G 40
#define HEADER_BG_B 40
#define EDITOR_SPLIT_GAP 4
#define EDITOR_SPLIT_MIN_CHILD_W 220
#define EDITOR_SPLIT_MIN_CHILD_H 140
#define EDITOR_MAX_LEAF_VIEWS 8
#define EDITOR_LINE_HEIGHT 17
#define EDITOR_CONTENT_TOP_PADDING 22
#define EDITOR_TAB_GAP 4
#define EDITOR_TAB_HEIGHT 18
#define EDITOR_TAB_TEXT_PAD 6
#define EDITOR_TAB_MIN_W 44
#define EDITOR_TAB_MAX_W_INACTIVE 132
#define EDITOR_TAB_MAX_W_ACTIVE 176
#define EDITOR_TAB_SCROLL_STEP 48
#define EDITOR_TAB_CLOSE_BTN_SIZE 18
#define EDITOR_TAB_CLOSE_BTN_MARGIN 4
#define EDITOR_LINE_NUMBER_GUTTER_W 18



#ifndef MAX_TABS_PER_VIEW
#define MAX_TABS_PER_VIEW 16
#endif

struct UIPane;

typedef enum {
    EDITOR_RENDER_REAL = 0,
    EDITOR_RENDER_PROJECTION
} EditorRenderSource;

typedef struct {
    char** lines;
    int lineCount;
    int* projectedToRealLine;
    int* projectedToRealCol;
    int* realMatchLines;
    int realMatchCount;
    uint64_t buildStamp;
} SearchProjection;

typedef struct OpenFile{
    int refCount;
    char* filePath;

    EditorBuffer* buffer;
    EditorState state;

    bool isModified;
    bool showSavedTag;
    Uint32 savedTagTimestamp;

    void* undoStack;  
    bool pendingTextEdit;

    uint64_t bufferVersion;
    EditorRenderSource renderSource;
    SearchProjection projection;
} OpenFile;



typedef enum {
    VIEW_LEAF,    // Normal editor with tabs
    VIEW_SPLIT    // A split view with two children
} EditorViewType;

typedef enum {
    SPLIT_HORIZONTAL, // Top/Bottom
    SPLIT_VERTICAL    // Left/Right
} SplitOrientation;

typedef struct {
    struct EditorView* leaf;
    struct EditorView* parent;
    bool isLeftChild;
} SplitTarget;



typedef struct EditorView {
    EditorViewType type;
    struct UIPane* parentPane;

    // If type == VIEW_LEAF:
    OpenFile** openFiles;
    int fileCount;
    int fileCapacity;
    int activeTab;
    int tabScrollX;

    // If type == VIEW_SPLIT:
    SplitOrientation splitType;
    float splitRatio;  // Portion of split axis assigned to childA (0..1, clamped)
    struct EditorView* childA;
    struct EditorView* childB;
    bool ownsFileData;

    // Layout info (for render size)
    int x, y, w, h;
} EditorView;




void layoutSplitChildren(EditorView* view);
void performEditorLayout(EditorView* view, int x, int y, int w, int h);


void resetViewCounters(void);
void setActiveEditorView(EditorView* view);
void setParentPaneForView(EditorView* view, struct UIPane* pane);

EditorView* findLeafUnderCursor(EditorView* root, int mouseX, int mouseY);
SplitTarget findSplittableLeafBreadthFirst(EditorView* root);
SplitTarget findSplittableLeafWithParent(EditorView* view, EditorView* parent, bool isLeft);
EditorView* findNextLeaf(EditorView* view);


// Core management
EditorView* createEditorView(void);
void addEditorView(EditorView* root, struct UIPane* pane);
bool splitEditorView(EditorView* root, EditorView* targetLeaf, struct UIPane* pane, SplitOrientation orientation);
bool closeEmptyEditorLeaf(EditorView* root, EditorView* leaf);
bool collapseEditorLeaf(EditorView* root, EditorView* leaf);
void destroyEditorView(EditorView* view);
void updateActiveEditorViewFromMouse(int mouseX, int mouseY);
void bindEditorViewToEditorPane(EditorView* savedView, struct UIPane** panes, int paneCount);


// New helper creation methods
EditorView* createLeafView(void);
EditorView* createSplitView(SplitOrientation splitType);
EditorView* cloneLeafView(EditorView* src, SplitOrientation parentSplit);




// Tab management
void closeFileInAllViews(EditorView* view, const char* filePath);
void closeTab(EditorView* view, int index);
void switchTab(EditorView* view, int direction); // -1 = left, +1 = right




// File state management
const char* getFileName(const char* path);
void markFileAsModified(OpenFile* file);

// File lifecycle
OpenFile* openFileInView(EditorView* view, const char* filePath);
void reloadOpenFileFromDisk(OpenFile* file);  // Optional stub

OpenFile* getActiveOpenFile(EditorView* view);
void editor_projection_reset(SearchProjection* projection);
void editor_projection_free(SearchProjection* projection);
void editor_invalidate_file_projection(OpenFile* file);
void editor_set_file_render_source(OpenFile* file, EditorRenderSource source);
bool editor_file_projection_active(const OpenFile* file);
void editor_sync_active_file_projection_mode(void);
bool editor_projection_map_row_to_source(const OpenFile* file,
                                         int projectedRow,
                                         int* outSourceRow,
                                         int* outSourceCol);
void editor_projection_set_scope_all_open_files(bool enabled);
bool editor_projection_scope_all_open_files(void);


#endif // EDITOR_VIEW_H
