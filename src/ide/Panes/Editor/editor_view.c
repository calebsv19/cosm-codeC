
#include <assert.h>
#include "app/GlobalInfo/core_state.h"
#include "core/Watcher/file_watcher.h"
#include "core/Analysis/fisics_bridge.h"


#include "engine/Render/render_pipeline.h"
#include "ide/Panes/Editor/editor_view.h"
#include "ide/Panes/Editor/editor_projection.h"
#include "ide/Panes/Editor/undo_stack.h"
#include "ide/Panes/ControlPanel/control_panel.h"
#include "ide/Panes/Editor/editor_state.h"
#include "ide/Panes/Editor/editor_view_state.h"
#include "app/GlobalInfo/project.h"
#include "editor.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>



#define INITIAL_TAB_CAPACITY 8 
#define TAB_WIDTH 120
#define TAB_PADDING 8


static EditorState fallbackEditorState = {0};
static bool g_projection_scope_all_open_files = true;

static float clampf_local(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static int compute_split_a_size(EditorView* split, int totalAxis, int minChildAxis) {
    if (!split) return 0;

    int available = totalAxis - EDITOR_SPLIT_GAP;
    if (available <= 0) {
        split->splitRatio = 0.5f;
        return 0;
    }

    if (available < (minChildAxis * 2)) {
        int half = available / 2;
        split->splitRatio = (float)half / (float)available;
        return half;
    }

    split->splitRatio = clampf_local(split->splitRatio, 0.05f, 0.95f);
    int aSize = (int)(split->splitRatio * (float)available + 0.5f);

    int minA = minChildAxis;
    int maxA = available - minChildAxis;
    if (aSize < minA) aSize = minA;
    if (aSize > maxA) aSize = maxA;

    split->splitRatio = (float)aSize / (float)available;
    return aSize;
}

static void apply_split_child_layout(EditorView* view) {
    if (!view || view->type != VIEW_SPLIT || !view->childA || !view->childB) return;

    if (view->splitType == SPLIT_VERTICAL) {
        int aWidth = compute_split_a_size(view, view->w, EDITOR_SPLIT_MIN_CHILD_W);
        int available = view->w - EDITOR_SPLIT_GAP;
        if (available < 0) available = 0;
        int bWidth = available - aWidth;
        if (bWidth < 0) bWidth = 0;

        view->childA->x = view->x;
        view->childA->y = view->y;
        view->childA->w = aWidth;
        view->childA->h = view->h;

        view->childB->x = view->x + aWidth + EDITOR_SPLIT_GAP;
        view->childB->y = view->y;
        view->childB->w = bWidth;
        view->childB->h = view->h;
    } else {
        int aHeight = compute_split_a_size(view, view->h, EDITOR_SPLIT_MIN_CHILD_H);
        int available = view->h - EDITOR_SPLIT_GAP;
        if (available < 0) available = 0;
        int bHeight = available - aHeight;
        if (bHeight < 0) bHeight = 0;

        view->childA->x = view->x;
        view->childA->y = view->y;
        view->childA->w = view->w;
        view->childA->h = aHeight;

        view->childB->x = view->x;
        view->childB->y = view->y + aHeight + EDITOR_SPLIT_GAP;
        view->childB->w = view->w;
        view->childB->h = bHeight;
    }
}

static void editor_release_open_file(OpenFile* file) {
    if (!file) return;
    if (file->refCount > 0) {
        file->refCount--;
        if (file->refCount > 0) return;
    }

    unwatchFile(file);
    clearUndoHistory(file);
    free(file->undoStack);
    file->undoStack = NULL;
    editor_projection_free(&file->projection);
    if (file->buffer) {
        freeEditorBuffer(file->buffer);
        file->buffer = NULL;
    }
    free(file->filePath);
    file->filePath = NULL;
    free(file);
}

void editor_projection_reset(SearchProjection* projection) {
    if (!projection) return;
    projection->lines = NULL;
    projection->lineCount = 0;
    projection->projectedToRealLine = NULL;
    projection->projectedToRealCol = NULL;
    projection->realMatchLines = NULL;
    projection->realMatchCount = 0;
    projection->buildStamp = 0;
}

void editor_projection_free(SearchProjection* projection) {
    if (!projection) return;
    const int maxReasonableRows = 200000;
    int lineCount = projection->lineCount;
    if (lineCount < 0 || lineCount > maxReasonableRows) {
        fprintf(stderr, "[Projection] Ignoring suspicious lineCount=%d during free\n", projection->lineCount);
        lineCount = 0;
    }

    uintptr_t linesPtr = (uintptr_t)projection->lines;
    if (projection->lines && linesPtr >= 4096u) {
        for (int i = 0; i < lineCount; ++i) {
            uintptr_t rowPtr = (uintptr_t)projection->lines[i];
            if (rowPtr >= 4096u) {
                free(projection->lines[i]);
            }
        }
        free(projection->lines);
    } else if (projection->lines) {
        fprintf(stderr, "[Projection] Ignoring suspicious lines pointer=%p during free\n",
                (void*)projection->lines);
    }

    uintptr_t realLinePtr = (uintptr_t)projection->projectedToRealLine;
    if (projection->projectedToRealLine && realLinePtr >= 4096u) {
        free(projection->projectedToRealLine);
    } else if (projection->projectedToRealLine) {
        fprintf(stderr, "[Projection] Ignoring suspicious realLine pointer=%p during free\n",
                (void*)projection->projectedToRealLine);
    }

    uintptr_t realColPtr = (uintptr_t)projection->projectedToRealCol;
    if (projection->projectedToRealCol && realColPtr >= 4096u) {
        free(projection->projectedToRealCol);
    } else if (projection->projectedToRealCol) {
        fprintf(stderr, "[Projection] Ignoring suspicious realCol pointer=%p during free\n",
                (void*)projection->projectedToRealCol);
    }

    uintptr_t matchesPtr = (uintptr_t)projection->realMatchLines;
    if (projection->realMatchLines && matchesPtr >= 4096u) {
        free(projection->realMatchLines);
    } else if (projection->realMatchLines) {
        fprintf(stderr, "[Projection] Ignoring suspicious realMatch pointer=%p during free\n",
                (void*)projection->realMatchLines);
    }
    editor_projection_reset(projection);
}

void editor_invalidate_file_projection(OpenFile* file) {
    if (!file) return;
    editor_projection_free(&file->projection);
}

void editor_set_file_render_source(OpenFile* file, EditorRenderSource source) {
    if (!file) return;
    file->renderSource = source;
}

bool editor_file_projection_active(const OpenFile* file) {
    return file && file->renderSource == EDITOR_RENDER_PROJECTION;
}

bool editor_projection_map_row_to_source(const OpenFile* file,
                                         int projectedRow,
                                         int* outSourceRow,
                                         int* outSourceCol) {
    if (!file || !editor_file_projection_active(file)) return false;
    if (!file->projection.projectedToRealLine || file->projection.lineCount <= 0) return false;
    if (projectedRow < 0 || projectedRow >= file->projection.lineCount) return false;

    int row = file->projection.projectedToRealLine[projectedRow];
    int col = (file->projection.projectedToRealCol && projectedRow < file->projection.lineCount)
                  ? file->projection.projectedToRealCol[projectedRow]
                  : 0;

    if (row < 0) {
        int above = projectedRow - 1;
        while (above >= 0 && file->projection.projectedToRealLine[above] < 0) {
            above--;
        }
        if (above >= 0) {
            row = file->projection.projectedToRealLine[above];
            col = (file->projection.projectedToRealCol && above < file->projection.lineCount)
                      ? file->projection.projectedToRealCol[above]
                      : 0;
        }
    }
    if (row < 0) {
        int below = projectedRow + 1;
        while (below < file->projection.lineCount &&
               file->projection.projectedToRealLine[below] < 0) {
            below++;
        }
        if (below < file->projection.lineCount) {
            row = file->projection.projectedToRealLine[below];
            col = (file->projection.projectedToRealCol && below < file->projection.lineCount)
                      ? file->projection.projectedToRealCol[below]
                      : 0;
        }
    }

    if (row < 0) return false;
    if (col < 0) col = 0;
    if (outSourceRow) *outSourceRow = row;
    if (outSourceCol) *outSourceCol = col;
    return true;
}

void editor_projection_set_scope_all_open_files(bool enabled) {
    g_projection_scope_all_open_files = enabled;
}

bool editor_projection_scope_all_open_files(void) {
    return g_projection_scope_all_open_files;
}



// 		RENDER METHODS


void layoutSplitChildren(EditorView* view) {
    apply_split_child_layout(view);
}


// 		RENDER METHODS
// ==========================================
// 		EDITOR VIEW MECHANICS






EditorView* createSplitView(SplitOrientation splitType) {
    EditorView* view = malloc(sizeof(EditorView));
    if (!view) return NULL;

    view->type = VIEW_SPLIT;
    view->openFiles = NULL;
    view->fileCount = 0;
    view->fileCapacity = 0;
    view->activeTab = -1;

    view->ownsFileData = false;
    view->childA = NULL;
    view->childB = NULL;
    view->splitType = splitType;
    view->splitRatio = 0.5f;
    view->x = view->y = view->w = view->h = 0;

    return view;
}


void addEditorView(EditorView* root, UIPane* pane) {
    IDECoreState* core = getCoreState();
    if (!root) return;

    EditorView* targetLeaf = core ? core->activeEditorView : NULL;
    if (!targetLeaf || targetLeaf->type != VIEW_LEAF) {
        SplitTarget fallback = findSplittableLeafBreadthFirst(root);
        targetLeaf = fallback.leaf;
    }
    (void)splitEditorView(root, targetLeaf, pane, SPLIT_VERTICAL);
}

static bool find_target_with_parent(EditorView* node,
                                    EditorView* target,
                                    EditorView* parent,
                                    bool isLeftChild,
                                    SplitTarget* out) {
    if (!node || !target || !out) return false;
    if (node == target) {
        out->leaf = node;
        out->parent = parent;
        out->isLeftChild = isLeftChild;
        return true;
    }
    if (node->type != VIEW_SPLIT) return false;
    return find_target_with_parent(node->childA, target, node, true, out) ||
           find_target_with_parent(node->childB, target, node, false, out);
}

static int count_leaf_nodes(const EditorView* view) {
    if (!view) return 0;
    if (view->type == VIEW_LEAF) return 1;
    return count_leaf_nodes(view->childA) + count_leaf_nodes(view->childB);
}

bool splitEditorView(EditorView* root, EditorView* targetLeaf, UIPane* pane, SplitOrientation orientation) {
    IDECoreState* core = getCoreState();
    if (!core || !root || !targetLeaf || targetLeaf->type != VIEW_LEAF) return false;
    if (core->editorViewCount >= EDITOR_MAX_LEAF_VIEWS) return false;

    SplitTarget targetInfo = {0};
    if (!find_target_with_parent(root, targetLeaf, NULL, false, &targetInfo)) {
        return false;
    }

    bool activeWasTarget = (core->activeEditorView == targetLeaf);
    EditorView* cloned = cloneLeafView(targetLeaf, orientation);
    if (!cloned) return false;
    cloned->parentPane = pane;

    EditorView* newView = createEditorView();
    if (!newView) {
        free(cloned->openFiles);
        free(cloned);
        return false;
    }
    newView->parentPane = pane;

    EditorView* split = createSplitView(orientation);
    if (!split) {
        free(cloned->openFiles);
        free(cloned);
        destroyEditorView(newView);
        return false;
    }
    split->childA = cloned;
    split->childB = newView;
    split->parentPane = pane;

    if (targetInfo.parent) {
        if (targetInfo.isLeftChild) {
            targetInfo.parent->childA = split;
        } else {
            targetInfo.parent->childB = split;
        }
    } else {
        root->type = VIEW_SPLIT;
        root->splitType = split->splitType;
        root->splitRatio = split->splitRatio;
        root->childA = split->childA;
        root->childB = split->childB;
        root->openFiles = NULL;
        root->fileCount = 0;
        root->fileCapacity = 0;
        root->activeTab = -1;
        root->tabScrollX = 0;
        root->ownsFileData = false;
        free(split);
    }

    core->editorViewCount++;
    setParentPaneForView(root, pane ? pane : targetLeaf->parentPane);
    rebuildLeafHitboxes(core->persistentEditorView);
    if (activeWasTarget) {
        setActiveEditorView(cloned);
    }
    return true;
}




SplitTarget findSplittableLeafBreadthFirst(EditorView* root) {
    SplitTarget result = { .leaf = NULL, .parent = NULL, .isLeftChild = false };
    if (!root) return result;

    EditorView* queue[32]; // simple queue, max 32 nodes
    EditorView* parents[32];
    bool isLeft[32];
    int front = 0, back = 0;

    queue[back] = root;
    parents[back] = NULL;
    isLeft[back] = false;
    back++;

    while (front < back) {
        EditorView* current = queue[front];
        EditorView* parent  = parents[front];
        bool leftFlag       = isLeft[front];
        front++;

        if (current->type == VIEW_LEAF) {
            result.leaf = current;
            result.parent = parent;
            result.isLeftChild = leftFlag;
            return result;
        }

        if (current->childA) {
            queue[back] = current->childA;
            parents[back] = current;
            isLeft[back] = true;
            back++;
        }

        if (current->childB) {
            queue[back] = current->childB;
            parents[back] = current;
            isLeft[back] = false;
            back++;
        }
    }

    return result; // fallback: no leaf found
}


SplitTarget findSplittableLeafWithParent(EditorView* view, EditorView* parent, bool isLeft) {
    if (!view) return (SplitTarget){NULL, NULL, false};

    if (view->type == VIEW_LEAF) {
        return (SplitTarget){view, parent, isLeft};
    }

    SplitTarget left = findSplittableLeafWithParent(view->childA, view, true);
    if (left.leaf) return left;

    return findSplittableLeafWithParent(view->childB, view, false);
}


EditorView* cloneLeafView(EditorView* src, SplitOrientation parentSplit){
    if (!src || src->type != VIEW_LEAF) return NULL;

    EditorView* view = malloc(sizeof(EditorView));
    if (!view) return NULL;

    view->type = VIEW_LEAF;
    view->fileCapacity = src->fileCapacity > 0 ? src->fileCapacity : INITIAL_TAB_CAPACITY;
    view->openFiles = calloc((size_t)view->fileCapacity, sizeof(OpenFile*));
    if (!view->openFiles) {
        free(view);
        return NULL;
    }

    view->fileCount = src->fileCount;
    view->activeTab = src->activeTab;
    view->tabScrollX = src->tabScrollX;
    view->ownsFileData = false;

    for (int i = 0; i < view->fileCount; i++) {
        OpenFile* file = src->openFiles ? src->openFiles[i] : NULL;
        view->openFiles[i] = file;
        if (file) {
            file->refCount++;
        }
    }

    view->childA = NULL;
    view->childB = NULL;
    view->splitType = (parentSplit == SPLIT_VERTICAL) ? SPLIT_HORIZONTAL : SPLIT_VERTICAL;
    view->parentPane = src->parentPane;
    view->x = view->y = view->w = view->h = 0;

    return view;
}



EditorView* findNextLeaf(EditorView* view) {
    if (!view) return NULL;
    if (view->type == VIEW_LEAF) return view;

    EditorView* found = findNextLeaf(view->childA);
    if (found) return found;

    return findNextLeaf(view->childB);
}


void performEditorLayout(EditorView* view, int x, int y, int w, int h) {
    if (!view) return;

    view->x = x;
    view->y = y;
    view->w = w;
    view->h = h;

    if (view->type == VIEW_SPLIT) {
        if (!view->childA || !view->childB) return;
        apply_split_child_layout(view);
        performEditorLayout(view->childA, view->childA->x, view->childA->y,
                            view->childA->w, view->childA->h);
        performEditorLayout(view->childB, view->childB->x, view->childB->y,
                            view->childB->w, view->childB->h);
    }


    IDECoreState* core = getCoreState();
    rebuildLeafHitboxes(core->persistentEditorView);
}







//              EDITOR VIEW MECHANICS
// 	===============================
// 		VIEW SETTING






void updateActiveEditorViewFromMouse(int mouseX, int mouseY) {
    IDECoreState* core = getCoreState();
    EditorView* root = core->persistentEditorView;
    if (!root) return;
    
    EditorView* hovered = hitTestLeaf(core->editorViewState, mouseX, mouseY);
    if (!hovered) {
        // Fallback for stale hitbox state between layout/render ticks.
        hovered = findLeafUnderCursor(root, mouseX, mouseY);
    }
    setHoveredEditorView(hovered);

    if (hovered && hovered->type == VIEW_LEAF) {
        setActiveEditorView(hovered);
    }
}


void setActiveEditorView(EditorView* view) {
    if (!view || view->type != VIEW_LEAF) return;

    getCoreState()->activeEditorView = view;

    editorBuffer = NULL;
    editorState = NULL;

    if (view->activeTab >= 0 && view->activeTab < view->fileCount) {
        OpenFile* current = view->openFiles[view->activeTab];
        if (current && current->buffer) {
            editorBuffer = current->buffer;
            editorState = &current->state;
        }
    } else {
        // Fallback state when no file is loaded
        editorBuffer = NULL;
        editorState = &fallbackEditorState;
    }

    editor_sync_active_file_projection_mode();
}



void setParentPaneForView(EditorView* view, UIPane* pane) {
    if (!view) return;

    if (view->type == VIEW_LEAF) {
        view->parentPane = pane;
    } else {
        setParentPaneForView(view->childA, pane);
        setParentPaneForView(view->childB, pane);
    }
}








//		VIEW SETTING
// 	===============================
// 		TAB LOGIC


void closeFileInAllViews(EditorView* view, const char* filePath) {
    if (!view || !filePath) return;

    if (view->type == VIEW_LEAF) {
        for (int i = view->fileCount - 1; i >= 0; i--) {
            OpenFile* file = view->openFiles[i];
            if (file && file->filePath && strcmp(file->filePath, filePath) == 0) {
                closeTab(view, i);
            }
        }
    } else {
        closeFileInAllViews(view->childA, filePath);
        closeFileInAllViews(view->childB, filePath);
    }
}


void closeTab(EditorView* view, int index) {
    if (!view || view->type != VIEW_LEAF || index < 0 || index >= view->fileCount)
        return;

    OpenFile* file = view->openFiles[index];
    if (file) {
        editor_release_open_file(file);
    }

    // Shift remaining tabs left
    for (int i = index; i < view->fileCount - 1; i++) {
        view->openFiles[i] = view->openFiles[i + 1];
    }

    view->fileCount--;
    if (view->fileCount >= 0) {
        view->openFiles[view->fileCount] = NULL;
    }

    // Clamp activeTab
    if (view->fileCount == 0) {
        view->activeTab = -1;
    } else if (view->activeTab >= view->fileCount) {
        view->activeTab = view->fileCount - 1;
    }

    IDECoreState* core = getCoreState();


    if (view == core->activeEditorView) {
        // Keep the leaf active even when the last tab closes so empty editor
        // views remain valid open targets for project-panel double-click/open.
        setActiveEditorView(view);
    }

    rebuildLeafHitboxes(getCoreState()->persistentEditorView);
}

bool closeEmptyEditorLeaf(EditorView* root, EditorView* leaf) {
    IDECoreState* core = getCoreState();
    if (!core || !root || !leaf || leaf->type != VIEW_LEAF) return false;
    if (leaf->fileCount > 0) return false;
    if (root == leaf) return false;

    SplitTarget target = {0};
    if (!find_target_with_parent(root, leaf, NULL, false, &target) || !target.parent) {
        return false;
    }

    EditorView* parent = target.parent;
    EditorView* sibling = target.isLeftChild ? parent->childB : parent->childA;
    if (!sibling) return false;

    SplitTarget parentInfo = {0};
    bool hasGrand = find_target_with_parent(root, parent, NULL, false, &parentInfo) && parentInfo.parent;

    if (hasGrand) {
        if (parentInfo.isLeftChild) {
            parentInfo.parent->childA = sibling;
        } else {
            parentInfo.parent->childB = sibling;
        }

        free(leaf->openFiles);
        free(leaf);
        free(parent);
    } else {
        if (root->openFiles) {
            free(root->openFiles);
            root->openFiles = NULL;
        }
        *root = *sibling;
        free(sibling);
        free(leaf->openFiles);
        free(leaf);
    }

    if (core->editorViewCount > 1) {
        core->editorViewCount--;
    }

    EditorView* next = findNextLeaf(root);
    if (next) {
        setActiveEditorView(next);
    } else {
        core->activeEditorView = NULL;
        editorBuffer = NULL;
        editorState = NULL;
    }

    setParentPaneForView(root, core->editorPane);
    rebuildLeafHitboxes(core->persistentEditorView);
    return true;
}

bool collapseEditorLeaf(EditorView* root, EditorView* leaf) {
    IDECoreState* core = getCoreState();
    if (!core || !root || !leaf || leaf->type != VIEW_LEAF) return false;
    if (root == leaf) return false;

    SplitTarget target = {0};
    if (!find_target_with_parent(root, leaf, NULL, false, &target) || !target.parent) {
        return false;
    }

    EditorView* parent = target.parent;
    EditorView* sibling = target.isLeftChild ? parent->childB : parent->childA;
    if (!sibling) return false;

    int removedLeaves = count_leaf_nodes(sibling);

    SplitTarget parentInfo = {0};
    bool hasGrand = find_target_with_parent(root, parent, NULL, false, &parentInfo) && parentInfo.parent;

    if (hasGrand) {
        if (parentInfo.isLeftChild) {
            parentInfo.parent->childA = leaf;
        } else {
            parentInfo.parent->childB = leaf;
        }

        if (target.isLeftChild) {
            parent->childA = NULL;
        } else {
            parent->childB = NULL;
        }

        destroyEditorView(sibling);
        free(parent);

        if (core->editorViewCount > removedLeaves) {
            core->editorViewCount -= removedLeaves;
        } else {
            core->editorViewCount = 1;
        }

        setParentPaneForView(root, core->editorPane);
        setActiveEditorView(leaf);
        rebuildLeafHitboxes(core->persistentEditorView);
        return true;
    }

    if (target.isLeftChild) {
        parent->childA = NULL;
    } else {
        parent->childB = NULL;
    }

    destroyEditorView(sibling);

    if (root->openFiles) {
        free(root->openFiles);
        root->openFiles = NULL;
    }
    *root = *leaf;
    free(leaf);

    if (core->editorViewCount > removedLeaves) {
        core->editorViewCount -= removedLeaves;
    } else {
        core->editorViewCount = 1;
    }

    setParentPaneForView(root, core->editorPane);
    setActiveEditorView(root);
    rebuildLeafHitboxes(core->persistentEditorView);
    return true;
}


void switchTab(EditorView* view, int direction) {
    if (!view || view->type != VIEW_LEAF || view->fileCount <= 0)
        return;

    if (view->activeTab >= 0 && view->activeTab < view->fileCount) {
        OpenFile* oldFile = view->openFiles[view->activeTab];
        oldFile->showSavedTag = false;
    }

    view->activeTab += direction;

    if (view->activeTab < 0)
        view->activeTab = view->fileCount - 1;
    else if (view->activeTab >= view->fileCount)
        view->activeTab = 0;

    // Core-aware active view comparison
    IDECoreState* core = getCoreState();
    if (view == core->activeEditorView) {
        setActiveEditorView(view);
    }
}




// 		TAB LOGIC
//  	================================
// 		MEM MANAGEMENT



void bindEditorViewToEditorPane(EditorView* savedView, UIPane** panes, int paneCount) {
    IDECoreState* core = getCoreState();
    core->editorPane = NULL;

    for (int i = 0; i < paneCount; i++) {
        if (panes[i]->role == PANE_ROLE_EDITOR) {
            core->editorPane = panes[i];
            UIPane* editorPane = core->editorPane;

            if (!editorPane->editorView && savedView) {
                editorPane->editorView = savedView;

                if (!savedView->parentPane) {
                    setParentPaneForView(editorPane->editorView, editorPane);
                }
            }

	    if (!editorPane->inputHandler) {
	        editorPane->inputHandler = &editorInputHandler;
	    }


            break;
        }
    }

    if (!core->editorPane) {
        printf("[ERROR] editorPane not found after layout.\n");
    } else if (savedView) {
	if (!core->persistentEditorView) {
            core->persistentEditorView = savedView;
        }

        EditorView* focus = core->activeEditorView;
        if (!focus || focus->type != VIEW_LEAF) {
            focus = savedView;
            if (focus && focus->type != VIEW_LEAF) {
                focus = findNextLeaf(focus);
            }

            if (focus) {
                setActiveEditorView(focus);
            }
        }
    }
}


EditorView* createLeafView(void) {
    EditorView* view = malloc(sizeof(EditorView));
    if (!view) return NULL;

    view->type = VIEW_LEAF;

    view->openFiles = malloc(sizeof(OpenFile*) * INITIAL_TAB_CAPACITY);
    if (!view->openFiles) {
        free(view);
        return NULL;
    }

    view->fileCount = 0;
    view->fileCapacity = INITIAL_TAB_CAPACITY;
    view->activeTab = -1;
    view->tabScrollX = 0;
    view->ownsFileData = true; // Leaf views initially own their file data

    view->childA = NULL;
    view->childB = NULL;
    view->splitType = SPLIT_VERTICAL; // Default; irrelevant for leaf but safe
    view->splitRatio = 0.5f;

    view->x = view->y = view->w = view->h = 0;

    if (!getCoreState()->persistentEditorView) {
	    getCoreState()->persistentEditorView = view;
    }
    
    return view;
}


EditorView* createEditorView(void) {
    return createLeafView();
}


void destroyEditorView(EditorView* view) {
    if (!view) return;

    if (view->type == VIEW_SPLIT) {
        if (view->childA) destroyEditorView(view->childA);
        if (view->childB) destroyEditorView(view->childB);
    } else if (view->type == VIEW_LEAF) {
        if (view->openFiles) {
            for (int i = 0; i < view->fileCount; i++) {
                if (view->openFiles[i]) {
                    editor_release_open_file(view->openFiles[i]);
                }
            }
            free(view->openFiles);
        }
    }

    free(view);
}





//		MEM MANAGEMENT
//	======================================
//		FILE MANAGEMENT



const char* getFileName(const char* path) {
    const char* slash = strrchr(path, '/');
    return (slash) ? slash + 1 : path;
}

void markFileAsModified(OpenFile* file) {
    if (!file) return;
    file->isModified = true;
    file->bufferVersion++;
    editor_invalidate_file_projection(file);
}

OpenFile* openFileInView(EditorView* view, const char* filePath) {
    if (!view || !filePath) return NULL;
    
    // Prevent duplicate opens
    for (int i = 0; i < view->fileCount; i++) {
        if (strcmp(view->openFiles[i]->filePath, filePath) == 0) {
            view->activeTab = i;
            setActiveEditorView(view);
            return view->openFiles[i];
        }
    }
    
    // Expand capacity if needed
    if (view->fileCount >= view->fileCapacity) {
        view->fileCapacity *= 2;
        view->openFiles = realloc(view->openFiles, sizeof(OpenFile*) * view->fileCapacity);
    }
    
    // Create new OpenFile
    OpenFile* file = calloc(1, sizeof(OpenFile));
    if (!file) return NULL;
    
    file->refCount = 1;
    file->undoStack = NULL;
    file->filePath = strdup(filePath);
    file->buffer = loadEditorBuffer(filePath);
    if (!file->buffer) {
        free(file->filePath);
        free(file); 
        return NULL;
    }

    memset(&file->state, 0, sizeof(EditorState));
    file->state.draggingReturnedToPane = true;
    file->isModified = false;
    file->bufferVersion = 1;
    file->renderSource = EDITOR_RENDER_REAL;
    editor_projection_reset(&file->projection);
    
    // Add to list
    view->openFiles[view->fileCount] = file;
    view->activeTab = view->fileCount;
    view->fileCount++;
    
    watchFile(file);
    
    setActiveEditorView(view);
    
    return file;
}

void reloadOpenFileFromDisk(OpenFile* file) {
    if (!file || !file->filePath) return;

    EditorBuffer* newBuffer = loadEditorBuffer(file->filePath);
    if (!newBuffer) {
        printf("[Watcher] Failed to reload: %s\n", file->filePath);
        return;
    }

    // Replace old buffer safely
    freeEditorBuffer(file->buffer);
    file->buffer = newBuffer;
    file->bufferVersion++;
    editor_invalidate_file_projection(file);

    // Optionally reset state or keep old scroll/cursor
    // memset(&file->state, 0, sizeof(EditorState)); ← only if you want to reset

    file->isModified = false;
    file->showSavedTag = false;

    printf("[Watcher] Reloaded: %s\n", file->filePath);
}

OpenFile* getActiveOpenFile(EditorView* view) {
    if (!view || view->type != VIEW_LEAF) return NULL;
    if (view->activeTab < 0 || view->activeTab >= view->fileCount) return NULL;
    return view->openFiles[view->activeTab];
}

static void sync_projection_for_file(OpenFile* file,
                                     bool applySearchMode,
                                     bool projectionRenderEnabled,
                                     const char* query,
                                     const SymbolFilterOptions* options) {
    if (!file) return;
    if (!applySearchMode) {
        editor_set_file_render_source(file, EDITOR_RENDER_REAL);
        editor_invalidate_file_projection(file);
        return;
    }
    editor_set_file_render_source(file,
                                  projectionRenderEnabled
                                      ? EDITOR_RENDER_PROJECTION
                                      : EDITOR_RENDER_REAL);
    editor_projection_rebuild(file, query, options);
}

static bool path_is_in_active_project(const char* filePath) {
    if (!filePath || !filePath[0] || !projectPath[0]) return false;
    size_t rootLen = strlen(projectPath);
    if (rootLen == 0) return false;
    if (strncmp(filePath, projectPath, rootLen) != 0) return false;
    char boundary = filePath[rootLen];
    return boundary == '\0' || boundary == '/';
}

static void sync_projection_for_view_tree(EditorView* view,
                                          OpenFile* activeFile,
                                          bool enableEditorTarget,
                                          bool queryActive,
                                          bool scopeProjectFiles,
                                          bool projectionRenderEnabled,
                                          const char* query,
                                          const SymbolFilterOptions* options) {
    if (!view) return;
    if (view->type == VIEW_SPLIT) {
        sync_projection_for_view_tree(view->childA,
                                      activeFile,
                                      enableEditorTarget,
                                      queryActive,
                                      scopeProjectFiles,
                                      projectionRenderEnabled,
                                      query,
                                      options);
        sync_projection_for_view_tree(view->childB,
                                      activeFile,
                                      enableEditorTarget,
                                      queryActive,
                                      scopeProjectFiles,
                                      projectionRenderEnabled,
                                      query,
                                      options);
        return;
    }
    if (view->type != VIEW_LEAF || !view->openFiles || view->fileCount <= 0) return;

    for (int i = 0; i < view->fileCount; ++i) {
        OpenFile* file = view->openFiles[i];
        bool inScope = scopeProjectFiles ? path_is_in_active_project(file ? file->filePath : NULL)
                                         : (file == activeFile);
        bool applySearchMode = enableEditorTarget && queryActive && inScope;
        sync_projection_for_file(file,
                                 applySearchMode,
                                 projectionRenderEnabled,
                                 query,
                                 options);
    }
}

void editor_sync_active_file_projection_mode(void) {
    IDECoreState* core = getCoreState();
    if (!core) {
        return;
    }

    const char* query = control_panel_get_search_query();
    bool queryActive = control_panel_is_search_enabled() && (query && query[0] != '\0');
    bool editorTargetEnabled = control_panel_target_editor_enabled();
    bool projectionRenderEnabled =
        control_panel_get_editor_view_mode() == CONTROL_EDITOR_VIEW_PROJECTION;
    bool scopeProjectFiles =
        control_panel_get_search_scope() == CONTROL_SEARCH_SCOPE_PROJECT_FILES;
    SymbolFilterOptions options = {0};
    control_panel_get_search_filter_options(&options);

    OpenFile* activeFile = NULL;
    if (core->activeEditorView && core->activeEditorView->type == VIEW_LEAF) {
        activeFile = getActiveOpenFile(core->activeEditorView);
    }

    EditorView* root = core->persistentEditorView ? core->persistentEditorView : core->activeEditorView;
    sync_projection_for_view_tree(root,
                                  activeFile,
                                  editorTargetEnabled,
                                  queryActive,
                                  scopeProjectFiles,
                                  projectionRenderEnabled,
                                  query,
                                  &options);
}
