#include "tool_tasks.h"
#include "engine/Render/render_pipeline.h"
#include "engine/Render/render_text_helpers.h"
#include "core/InputManager/UserInput/rename_flow.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Main task tree structure
TaskNode** taskRoots = NULL;
int taskRootCount = 0;

// Current interaction state
TaskNode* selectedTask = NULL;
TaskNode* hoveredTask = NULL;

// Internal mouse tracking



// Used during recursive traversal
static int currentY = 0;
bool includeNestLabel = false;


TaskNode* editingTask = NULL;
char taskEditingBuffer[128] = "";



// ==================================
// 		INIT

void initTaskPanel() {
    if (taskRootCount > 0) {
        printf("[initTaskPanel] Skipping default task setup — task tree already loaded (%d roots).\n", taskRootCount);
        return;
    }

    printf("[initTaskPanel] No saved task tree found — creating default groups.\n");

    if (!taskRoots) {
        taskRoots = calloc(MAX_TASK_ROOTS, sizeof(TaskNode*));
        if (!taskRoots) {
            fprintf(stderr, "[initTaskPanel] Failed to allocate taskRoots array.\n");
            return;
        }
    }

    TaskNode* editorGroup = createTaskNode("Editor Features", true, NULL);
    addTaskChild(editorGroup, createTaskNode("Refactor input system", false, editorGroup));
    addTaskChild(editorGroup, createTaskNode("Add undo/redo support", false, editorGroup));
    addTaskChild(editorGroup, createTaskNode("Make panel resizable", false, editorGroup));

    TaskNode* bugsGroup = createTaskNode("Bugs", true, NULL);
    addTaskChild(bugsGroup, createTaskNode("Fix asset loading memory leak", false, bugsGroup));
    addTaskChild(bugsGroup, createTaskNode("Incorrect scroll behavior on terminal", false, bugsGroup));

    taskRoots[0] = editorGroup;
    taskRoots[1] = bugsGroup;
    taskRootCount = 2;
}

void freeTaskTree(TaskNode* node) {
    for (int i = 0; i < node->childCount; i++) {
        freeTaskTree(node->children[i]);
    }
    free(node);
}



//              INIT
// ==================================
//              Helper Methods



// ========== Static Prototypes for Helpers ==========
static void clearAllTaskSelections(void);
static bool nodeHasChildren(TaskNode* node);
static void toggleTaskExpansion(TaskNode* node);
static void toggleTaskCompletion(TaskNode* node);

static void handleCommandAddTask(void);
static void handleCommandRemoveSelectedTask(void);

static int getTaskTreeStartY(UIPane* pane);
static void updateTaskTreeHover(int x, int startY, int mx, int my);
static void clearTaskNodeUIState(TaskNode* node, bool clearSelection);





static bool pointInRect(int px, int py, SDL_Rect rect) {
    return (px >= rect.x && px <= rect.x + rect.w &&
            py >= rect.y && py <= rect.y + rect.h);
}


static void clearTaskNodeUIState(TaskNode* node, bool clearSelection) {
    if (!node) {
        fprintf(stderr, "[Tasks] clearTaskNodeUIState called with NULL\n");
        return;
    }

    node->isHovered = false;
    if (clearSelection) {
        node->isSelected = false;
    }

    for (int i = 0; i < node->childCount; i++) {
        clearTaskNodeUIState(node->children[i], clearSelection);
    }
}




TaskNode* createTaskNode(const char* label, bool isGroup, TaskNode* parent) {
    TaskNode* node = calloc(1, sizeof(TaskNode));  // zero all memory
    if (!node) {
        fprintf(stderr, "[Tasks] Failed to allocate TaskNode for label: '%s'\n", label ? label : "(null)");
        return NULL;
    }

    // Sanitize label input
    if (label && label[0] != '\0') {
        strncpy(node->label, label, sizeof(node->label) - 1);
        node->label[sizeof(node->label) - 1] = '\0'; // null-terminate
    } else {
        node->label[0] = '\0'; // fallback to empty string
    }

    node->completed = false;
    node->isExpanded = true;
    node->isGroup = isGroup;
    node->parent = parent;
    node->depth = parent ? parent->depth + 1 : 0;

    return node;
}

void addTaskChild(TaskNode* parent, TaskNode* child) {
    if (!parent || !child) {
        fprintf(stderr, "[addTaskChild] Error: NULL parent or child\n");
        return;
    }

    if (parent->childCount >= MAX_CHILDREN_PER_TASK) {
        fprintf(stderr, "[addTaskChild] Error: Too many children on node '%s'\n", parent->label);
        return;
    }

    if (!child->label[0]) {
        fprintf(stderr, "[addTaskChild] Warning: Adding child with empty label to '%s'\n", parent->label);
    }

    child->parent = parent;
    child->depth = parent->depth + 1;
    parent->children[parent->childCount++] = child;

    parent->isExpanded = true;
}


static TaskNode* resolveAddTarget(TaskNode* selected) {
    if (!selected) return NULL;

    // Add to group or expandable task
    if (selected->isGroup || selected->childCount > 0) {
        return selected;
    }

    // It's a leaf task → convert to expandable parent
    selected->isExpanded = true;
    return selected;
}




static void removeSelectedTaskAndReselect() {
    if (!selectedTask) return;

    TaskNode* parent = selectedTask->parent;
    TaskNode** siblings = parent ? parent->children : taskRoots;
    int* count = parent ? &parent->childCount : &taskRootCount;

    int selectedIndex = -1;
    for (int i = 0; i < *count; i++) {
        if (siblings[i] == selectedTask) {
            selectedIndex = i;
            break;
        }
    }

    if (selectedIndex != -1) {
        freeTaskTree(selectedTask);
        for (int j = selectedIndex; j < *count - 1; j++) {
            siblings[j] = siblings[j + 1];
        }
        (*count)--;

        if (*count > 0) {
            int newIndex = (selectedIndex < *count) ? selectedIndex : *count - 1;
            selectedTask = siblings[newIndex];
            selectedTask->isSelected = true;
        } else {
            selectedTask = NULL;
        }
    }
}




// 	=== Helper Methods ===
//  -------------------------------------------
// 	=== Input Handler ===



static void handleTaskTreeHover(TaskNode* node, int x, int mx, int my) {
    if (!node) {
        fprintf(stderr, "[Hover] NULL task node encountered, skipping...\n");
        return;
    }


    const int lineHeight = TASK_LINE_HEIGHT;
    const int expandWidth = 28;
    const int checkboxWidth = 28;
    const int spacing = 6;

    int indent = node->depth * TASK_INDENT_WIDTH;
    int drawX = x + indent;
    int drawY = currentY;

    bool isLineHovered = (my >= drawY && my < drawY + lineHeight);
    bool isXHovered = false;

    if (isLineHovered) {
        if (strlen(node->label) == 0) {
            isXHovered = true;  // highlight whole line
        } else {
            bool hasExpandIcon = (node->isGroup || node->childCount > 0);
            int usedExpandWidth = (hasExpandIcon && includeNestLabel) ? expandWidth : 0;
            int labelWidth = getTextWidth(node->label);
            int totalWidth = usedExpandWidth + checkboxWidth + labelWidth + spacing * 2;
            int boxX = drawX + expandWidth - usedExpandWidth - spacing;

            SDL_Rect labelBox = { boxX, drawY - 1, totalWidth, lineHeight };
            isXHovered = pointInRect(mx, my, labelBox);
        }

        if (isXHovered) {
            node->isHovered = true;
        } else {
            node->isHovered = false;
        }
    } else {
        node->isHovered = false;
    }

    currentY += lineHeight;

    if (node->isExpanded) {
        for (int i = 0; i < node->childCount; i++) {
            handleTaskTreeHover(node->children[i], x, mx, my);
        }
    }
}












static bool handleCommandClickTaskNode(TaskNode* node, int panelStartX, int mouseX, int mouseY, UIPane* pane) {
    const int lineHeight = TASK_LINE_HEIGHT;
    const int expandWidth = 28;
    const int checkboxWidth = 20;
    const int spacing = 6;

    int indent = node->depth * TASK_INDENT_WIDTH;
    int drawX = panelStartX + indent;
    int drawY = currentY;

    bool wasClicked = false;

    bool hasExpandIcon = (node->isGroup || node->childCount > 0);
    int usedExpandWidth = (hasExpandIcon && includeNestLabel) ? expandWidth : 0;

    int labelWidth = getTextWidth(node->label);
    int totalWidth = usedExpandWidth + checkboxWidth + labelWidth + spacing * 2;
    int boxX = drawX + expandWidth - usedExpandWidth - spacing;

    // === Define clickable regions ===
    SDL_Rect expandBox  = { drawX - 7, drawY - 1, expandWidth, lineHeight };
    SDL_Rect checkBox   = { drawX + expandWidth - 5, drawY - 1, checkboxWidth, lineHeight };
    
    SDL_Rect labelBox;
    if (strlen(node->label) == 0) {
        // Wide clickable zone across pane
        labelBox.x = panelStartX;
        labelBox.y = drawY - 1;
        labelBox.w = pane->w;  // You may replace this with pane->w if needed
        labelBox.h = lineHeight;
    } else {
        labelBox.x = boxX;
        labelBox.y = drawY - 1;
        labelBox.w = totalWidth;
        labelBox.h = lineHeight;
    }

    // === Check hits ===
    bool inLabelBox  = pointInRect(mouseX, mouseY, labelBox);
    bool inExpandBox = hasExpandIcon && pointInRect(mouseX, mouseY, expandBox);
    bool inCheckBox  = pointInRect(mouseX, mouseY, checkBox);

    // === Click handling ===
    if (inLabelBox || inExpandBox || inCheckBox) {
        hoveredTask = node;

        if (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_LEFT)) {
            clearAllTaskSelections();
            selectedTask = node;
            node->isSelected = true;
            wasClicked = true;

            if (inExpandBox && nodeHasChildren(node)) {
                toggleTaskExpansion(node);
            } else if (inCheckBox) {
                toggleTaskCompletion(node);
            }
        }
    }

    currentY += lineHeight;

    if (node->isExpanded) {
        for (int i = 0; i < node->childCount; i++) {
            if (handleCommandClickTaskNode(node->children[i], panelStartX, mouseX, mouseY, pane)) {
                wasClicked = true;
            }
        }
    }

    return wasClicked;
}






static void clearAllTaskSelections(void) {
    for (int i = 0; i < taskRootCount; i++) {
	clearTaskNodeUIState(taskRoots[i], true);

    }
}

static void clearAllTaskHoverStates(void) {
    for (int i = 0; i < taskRootCount; i++) {
        clearTaskNodeUIState(taskRoots[i], false);
    }
}



static bool nodeHasChildren(TaskNode* node) {
    return node->isGroup || node->childCount > 0;
}


static void toggleTaskExpansion(TaskNode* node) {
    node->isExpanded = !node->isExpanded;
}

static void toggleTaskCompletion(TaskNode* node) {
    node->completed = !node->completed;
}






 

static bool tryClickAddTaskButton(int x, int y, int mx, int my) {
    SDL_Rect rect = { x, y, TASK_BUTTON_HEIGHT, TASK_BUTTON_HEIGHT };
//    printf("[Tasks] Add Btn Rect = { x=%d, y=%d, w=%d, h=%d }, Mouse = { x=%d, y=%d }\n",
//           rect.x, rect.y, rect.w, rect.h, mx, my);

    if (!pointInRect(mx, my, rect)) return false;

    handleCommandAddTask();
    return true;
}


static bool tryClickRemoveTaskButton(int x, int y, int mx, int my) {
    SDL_Rect rect = { x, y, TASK_BUTTON_HEIGHT, TASK_BUTTON_HEIGHT };
//    printf("[Tasks] Remove Btn Rect = { x=%d, y=%d, w=%d, h=%d }, Mouse = { x=%d, y=%d }\n",
//           rect.x, rect.y, rect.w, rect.h, mx, my);

    if (!pointInRect(mx, my, rect)) return false;

    if (selectedTask) {
        handleCommandRemoveSelectedTask();   
    }
    return true;
}



static int getTaskTreeStartY(UIPane* pane) {
    return pane->y + TASK_PANEL_TOP_PADDING + 2 * (TASK_BUTTON_HEIGHT + TASK_BUTTON_SPACING);
}


static void updateTaskTreeHover(int x, int startY, int mx, int my) {
    currentY = startY;
    for (int i = 0; i < taskRootCount; i++) {
	    if (!taskRoots[i]) {
	        fprintf(stderr, "[Hover] taskRoots[%d] is NULL during hover!\n", i);
	    } else {
	        handleTaskTreeHover(taskRoots[i], x, mx, my);
	    }
    }

}





void handleTaskMouseMotion(UIPane* pane, SDL_Event* event) {
    int mx = event->motion.x;
    int my = event->motion.y;

    int treeX = pane->x + TASK_PANEL_LEFT_PADDING;
    int treeY = getTaskTreeStartY(pane);

    clearAllTaskHoverStates();
    updateTaskTreeHover(treeX, treeY, mx, my);
}


static bool isTaskNameValid(const char* newName, void* context) {
    if (!newName || strlen(newName) == 0) return false;

    TaskNode* current = (TaskNode*)context;
    TaskNode* parent = current->parent;

    TaskNode** siblings = parent ? parent->children : taskRoots;
    int count = parent ? parent->childCount : taskRootCount;

    for (int i = 0; i < count; i++) {
        if (siblings[i] != current &&
            strncmp(siblings[i]->label, newName, sizeof(siblings[i]->label)) == 0) {
            return false; // name conflict
        }
    }

    return true;
}


static void handleTaskRenameCallback(const char* oldName, const char* newName, void* context) {
    TaskNode* node = (TaskNode*)context;
    if (!node || !newName) return;

    strncpy(node->label, newName, sizeof(node->label));
    node->label[sizeof(node->label) - 1] = '\0';

    printf("[Tasks] Renamed task: %s → %s\n", oldName, newName);
}



void handleTaskLeftClick(UIPane* pane, SDL_Event* event) {
    int mx = event->button.x;
    int my = event->button.y;

    int x = pane->x + TASK_PANEL_LEFT_PADDING;
    int y = pane->y + TASK_PANEL_TOP_PADDING;

    if (tryClickAddTaskButton(x, y, mx, my)) return;
    y += TASK_BUTTON_HEIGHT + TASK_BUTTON_SPACING;

    if (tryClickRemoveTaskButton(x, y, mx, my)) return;
    y += TASK_BUTTON_HEIGHT + TASK_BUTTON_SPACING;

    currentY = y;
    bool clickedNode = false;

    for (int i = 0; i < taskRootCount; i++) {
        if (handleCommandClickTaskNode(taskRoots[i], x, mx, my, pane)) {
            clickedNode = true;
        }
    }

    if (!clickedNode) {
        clearAllTaskSelections();
        selectedTask = NULL;
    }
}





static void handleCommandAddTask(void) {
    TaskNode* parent = resolveAddTarget(selectedTask);
    TaskNode* newTask = createTaskNode("", false, parent);
    if (!newTask) return;

    if (parent) {
        addTaskChild(parent, newTask);
    } else {
        // === Safe dynamic allocation for taskRoots
        if (!taskRoots) {
            taskRoots = calloc(MAX_TASK_ROOTS, sizeof(TaskNode*));
            if (!taskRoots) {
                fprintf(stderr, "[Tasks] Failed to allocate taskRoots array\n");
                return;
            }
        }

        if (taskRootCount >= MAX_TASK_ROOTS) {
            fprintf(stderr, "[Tasks] Cannot add more root tasks — MAX_TASK_ROOTS reached\n");
            return;
        }

        taskRoots[taskRootCount++] = newTask;
    }
}


static void handleCommandRemoveSelectedTask(void) {
    removeSelectedTaskAndReselect();
}








// 		HANDLE EVENTS
// ===========================================
// 		APIS



void taskPanelAddTask(UIPane* pane) {
    TaskNode* parent = resolveAddTarget(selectedTask);
    TaskNode* newTask = createTaskNode("New Task", false, parent);

    if (parent) {
        addTaskChild(parent, newTask);
    } else {
        taskRoots[taskRootCount++] = newTask;
    }
}
 

void taskPanelDeleteTask(UIPane* pane) {
    (void)pane;
    removeSelectedTaskAndReselect();
}


void taskPanelRenameTask(UIPane* pane) {
    (void)pane;
    if (selectedTask) {
	beginRename(
	    selectedTask->label,
	    handleTaskRenameCallback,
	    isTaskNameValid,
	    selectedTask
	);

    }
}


void taskPanelMoveTaskUp(UIPane* pane) {
    // Not yet implemented – add logic here later
    (void)pane;
    printf("[Tasks] Move task up (unimplemented)\n");
}

void taskPanelMoveTaskDown(UIPane* pane) {
    // Not yet implemented – add logic here later
    (void)pane;
    printf("[Tasks] Move task down (unimplemented)\n");
}







//              APIS
// ==================================

