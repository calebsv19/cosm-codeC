#ifndef EDITOR_VIEW_H
#define EDITOR_VIEW_H

#include "../GlobalInfo/core_state.h"
#include "editor.h"  // For EditorBuffer and EditorState
#include "Editor/editor_state.h"
#include "Editor/editor_buffer.h"

#include "Editor/Input/input_editor.h" // for &editorInputHandler


// Shared UI layout constants
#define EDITOR_PADDING 6
#define HEADER_HEIGHT 22
#define HEADER_BG_R 40
#define HEADER_BG_G 40
#define HEADER_BG_B 40



#ifndef MAX_TABS_PER_VIEW
#define MAX_TABS_PER_VIEW 16
#endif

struct UIPane;

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

    // If type == VIEW_SPLIT:
    SplitOrientation splitType;
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


#endif // EDITOR_VIEW_H

