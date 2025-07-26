
#include <assert.h>
#include "app/GlobalInfo/core_state.h"
#include "core/Watcher/file_watcher.h"


#include "engine/Render/render_pipeline.h"
#include "ide/Panes/Editor/editor_view.h"
#include "ide/Panes/Editor/editor_state.h"
#include "ide/Panes/Editor/editor_view_state.h"
#include "editor.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>



#define INITIAL_TAB_CAPACITY 8 
#define TAB_WIDTH 120
#define TAB_PADDING 8


static EditorState fallbackEditorState = {0};



// 		RENDER METHODS


void layoutSplitChildren(EditorView* view) {
    const int splitGap = 4;
    if (!view || view->type != VIEW_SPLIT || !view->childA || !view->childB) return;

    if (view->splitType == SPLIT_VERTICAL) {
        int childWidth = (view->w - splitGap) / 2;
        view->childA->x = view->x;
        view->childA->y = view->y;
        view->childA->w = childWidth;
        view->childA->h = view->h;

        view->childB->x = view->x + childWidth + splitGap;
        view->childB->y = view->y;
        view->childB->w = childWidth;
        view->childB->h = view->h;
    } else {
        int childHeight = (view->h - splitGap) / 2;
        view->childA->x = view->x;
        view->childA->y = view->y;
        view->childA->w = view->w;
        view->childA->h = childHeight;

        view->childB->x = view->x;
        view->childB->y = view->y + childHeight + splitGap;
        view->childB->w = view->w;
        view->childB->h = childHeight;
    }
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
    view->x = view->y = view->w = view->h = 0;

    return view;
}


void addEditorView(EditorView* root, UIPane* pane) {
    IDECoreState* core = getCoreState();
    if (!root || core->editorViewCount >= 4) return;

    // 1. Find a splittable leaf node using breadth-first search
    SplitTarget targetInfo = findSplittableLeafBreadthFirst(root);
    EditorView* leaf = targetInfo.leaf;

    if (!leaf || leaf->type != VIEW_LEAF) {
        fprintf(stderr, "[Error] No valid leaf to split.\n");
        return;
    }

    // 2. Clone the leaf (original contents)
    EditorView* cloned = cloneLeafView(leaf, root->splitType);
    if (!cloned) {
        fprintf(stderr, "[Error] Failed to clone leaf view.\n");
        return;
    }
    cloned->parentPane = pane;

    // 3. Create new blank view, sharing the same files
    EditorView* newView = createEditorView();
    if (!newView) {
        free(cloned);
        fprintf(stderr, "[Error] Failed to allocate new view.\n");
        return;
    }

    // 4. Shallow share the file list for now
    newView->openFiles = malloc(sizeof(OpenFile*) * INITIAL_TAB_CAPACITY);
    newView->fileCount = 0;
    newView->fileCapacity = INITIAL_TAB_CAPACITY;
    newView->activeTab = -1;
    newView->ownsFileData = true;
    newView->parentPane = pane;

    // 5. Create a new split container
    SplitOrientation newSplitType;
    if (targetInfo.parent) {
        // Alternate based on parent
        newSplitType = (targetInfo.parent->splitType == SPLIT_VERTICAL)
                   ? SPLIT_HORIZONTAL : SPLIT_VERTICAL;
    } else {
        // Root case: alternate based on editorViewCount as fallback
        newSplitType = (core->editorViewCount % 2 == 1)
                   ? SPLIT_VERTICAL : SPLIT_HORIZONTAL;
    }
    EditorView* split = createSplitView(newSplitType);
    if (!split) {
        free(cloned);
        free(newView);
        fprintf(stderr, "[Error] Failed to allocate split view.\n");
        return;
    }

    split->childA = cloned;
    split->childB = newView;

    // 6. Rewire the parent to point to the new split node
    if (targetInfo.parent) {
        if (targetInfo.isLeftChild) {
            targetInfo.parent->childA = split;
        } else {
            targetInfo.parent->childB = split;
        }
    } else {
	    root->type = VIEW_SPLIT;
	    root->splitType = split->splitType;
	    root->childA = split->childA;
	    root->childB = split->childB;
	
	    // Root no longer owns file data
	    root->openFiles = NULL;
	    root->fileCount = 0;
	    root->fileCapacity = 0;
	    root->activeTab = -1;
	    root->ownsFileData = false;
    }

   
    core->editorViewCount++;

    setParentPaneForView(root, leaf->parentPane);

    // printf("[Fix] Updated parentPane assignments after splitting editor tree.\n");

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
    view->openFiles = src->openFiles; // shallow copy
    view->fileCount = src->fileCount;
    view->fileCapacity = src->fileCapacity;
    view->activeTab = src->activeTab;
    view->ownsFileData = false;

    view->childA = NULL;
    view->childB = NULL;
    view->splitType = (parentSplit == SPLIT_VERTICAL) ? SPLIT_HORIZONTAL : SPLIT_VERTICAL;
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

    const int splitGap = 4;

    if (view->type == VIEW_SPLIT) {
        if (!view->childA || !view->childB) return;

        if (view->splitType == SPLIT_VERTICAL) {
            int childWidth = (w - splitGap) / 2;
            performEditorLayout(view->childA, x, y, childWidth, h);
            performEditorLayout(view->childB, x + childWidth + splitGap, y, childWidth, h);
        } else {
            int childHeight = (h - splitGap) / 2;
            performEditorLayout(view->childA, x, y, w, childHeight);
            performEditorLayout(view->childB, x, y + childHeight + splitGap, w, childHeight);
        }
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
    
//    rebuildLeafHitboxes(root);
    
    EditorViewState* vs = core->editorViewState;   
    for (int i = 0; i < vs->leafHitboxCount; i++) {
        LeafHitbox* hit = &vs->leafHitboxes[i];
        SDL_Rect rect = hit->rect;
        
        if (mouseX >= rect.x && mouseX < rect.x + rect.w &&
            mouseY >= rect.y && mouseY < rect.y + rect.h) {
            setActiveEditorView((EditorView*)hit->view);
            return;
        }
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
	unwatchFile(file);

        freeEditorBuffer(file->buffer);
        free(file->filePath);
        free(file);
    }

    // Shift remaining tabs left
    for (int i = index; i < view->fileCount - 1; i++) {
        view->openFiles[i] = view->openFiles[i + 1];
    }

    view->fileCount--;

    // Clamp activeTab
    if (view->fileCount == 0) {
        view->activeTab = -1;
    } else if (view->activeTab >= view->fileCount) {
        view->activeTab = view->fileCount - 1;
    }

    IDECoreState* core = getCoreState();


    if (view == core->activeEditorView) {
        if (view->activeTab >= 0 && view->activeTab < view->fileCount) {
            OpenFile* current = view->openFiles[view->activeTab];
            if (current && current->buffer) {
                editorBuffer = current->buffer;
                editorState = &current->state;
            } else {
                editorBuffer = NULL;
                editorState = NULL;
            }
        } else {
            // No valid tabs left in this view
            editorBuffer = NULL;
            editorState = NULL;
            core->activeEditorView = NULL;
        }
    }

    rebuildLeafHitboxes(getCoreState()->persistentEditorView);
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
        setActiveEditorView(savedView);
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
    view->ownsFileData = true; // Leaf views initially own their file data

    view->childA = NULL;
    view->childB = NULL;
    view->splitType = SPLIT_VERTICAL; // Default; irrelevant for leaf but safe

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
        if (view->ownsFileData && view->openFiles) {
            for (int i = 0; i < view->fileCount; i++) {
                if (view->openFiles[i]) {
		    unwatchFile(view->openFiles[i]);

                    freeEditorBuffer(view->openFiles[i]->buffer);
                    free(view->openFiles[i]->filePath);
                    free(view->openFiles[i]);
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
    if (file) file->isModified = true;
}

OpenFile* openFileInView(EditorView* view, const char* filePath) {
    if (!view || !filePath) return NULL;
    
    printf(" -> Opening %s into view %p\n", filePath, (void*)view);
    
    // Prevent duplicate opens
    for (int i = 0; i < view->fileCount; i++) {
        if (strcmp(view->openFiles[i]->filePath, filePath) == 0) {
            printf("[INFO] File already open in tab %d. Switching to it.\n", i);
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
    OpenFile* file = malloc(sizeof(OpenFile));
    if (!file) return NULL;
    
    file->refCount = 1;
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

    // Optionally reset state or keep old scroll/cursor
    // memset(&file->state, 0, sizeof(EditorState)); ← only if you want to reset

    file->isModified = false;
    file->showSavedTag = false;

    printf("[Watcher] Reloaded: %s\n", file->filePath);
}

