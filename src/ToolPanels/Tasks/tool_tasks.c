#include "tool_tasks.h"
#include "Render/render_pipeline.h"
#include "Render/render_text_helpers.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Main task tree structure
TaskNode* taskRoots[MAX_TASK_ROOTS];
int taskRootCount = 0;

// Current interaction state
TaskNode* selectedTask = NULL;
TaskNode* hoveredTask = NULL;

// Internal mouse tracking



// Used during recursive traversal
static int currentY = 0;


TaskNode* editingTask = NULL;
char taskEditingBuffer[128] = "";



// ==================================
// 		INIT

void initTaskPanel() {
    taskRootCount = 0;

    // Group: Editor Features
    TaskNode* editorGroup = createTaskNode("Editor Features", true, NULL);
    addTaskChild(editorGroup, createTaskNode("Refactor input system", false, editorGroup));
    addTaskChild(editorGroup, createTaskNode("Add undo/redo support", false, editorGroup));
    addTaskChild(editorGroup, createTaskNode("Make panel resizable", false, editorGroup));

    // Group: Bugs
    TaskNode* bugsGroup = createTaskNode("Bugs", true, NULL);
    addTaskChild(bugsGroup, createTaskNode("Fix asset loading memory leak", false, bugsGroup));
    addTaskChild(bugsGroup, createTaskNode("Incorrect scroll behavior on terminal", false, 
			bugsGroup));

    taskRoots[taskRootCount++] = editorGroup;
    taskRoots[taskRootCount++] = bugsGroup;
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
static bool isClickOnExpander(int clickX, int drawX);
static bool isClickOnCheckbox(int clickX, int drawX, int expandWidth, int checkboxWidth);
static void toggleTaskExpansion(TaskNode* node);
static void toggleTaskCompletion(TaskNode* node);

static void handleCommandAddTask(void);
static void handleCommandRemoveSelectedTask(void);

static int getTaskTreeStartY(UIPane* pane);
static void updateTaskTreeHover(int x, int startY, int mx, int my);
static void clearTaskNodeUIState(TaskNode* node, bool clearSelection);


static void handleClickTaskTree(int mx, int my, int x, int yStart);



static bool pointInRect(int px, int py, SDL_Rect rect) {
    return (px >= rect.x && px <= rect.x + rect.w &&
            py >= rect.y && py <= rect.y + rect.h);
}


static void clearTaskNodeUIState(TaskNode* node, bool clearSelection) {
    node->isHovered = false;
    if (clearSelection) {
        node->isSelected = false;
    }

    for (int i = 0; i < node->childCount; i++) {
        clearTaskNodeUIState(node->children[i], clearSelection);
    }
}




TaskNode* createTaskNode(const char* label, bool isGroup, TaskNode* parent) {
    TaskNode* node = malloc(sizeof(TaskNode));
    if (!node) return NULL;

    strncpy(node->label, label, sizeof(node->label));
    node->label[sizeof(node->label) - 1] = '\0'; // null-terminate safely

    node->completed = false;
    node->isExpanded = true;
    node->isGroup = isGroup;
    node->parent = parent;
    node->childCount = 0;
    node->depth = parent ? parent->depth + 1 : 0;

    node->isSelected = false;
    node->isHovered = false;

    for (int i = 0; i < MAX_CHILDREN_PER_TASK; i++) {
        node->children[i] = NULL;
    }

    return node;
}

void addTaskChild(TaskNode* parent, TaskNode* child) {
    if (!parent || !child || parent->childCount >= MAX_CHILDREN_PER_TASK) return;


    child->parent = parent;
    child->depth = parent->depth + 1;
    parent->children[parent->childCount++] = child;

    // Auto-expand parent on child insert
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


static void beginRenamingSelectedTask(void) {
    if (!selectedTask) return;

    editingTask = selectedTask;
    strncpy(taskEditingBuffer, selectedTask->label, sizeof(taskEditingBuffer));
    taskEditingBuffer[sizeof(taskEditingBuffer) - 1] = '\0';

    printf("[Tasks] Entered editing mode: %s\n", taskEditingBuffer);
}




// 	=== Helper Methods ===
//  -------------------------------------------
// 	=== Input Handler ===



static void handleTaskTreeHover(TaskNode* node, int x, int mx, int my) {
    int drawY = currentY;

    int lineHeight = TASK_LINE_HEIGHT;
    bool isHovered = (my >= drawY && my < drawY + lineHeight);
    if (isHovered) {
        node->isHovered = true;
    }

    currentY += lineHeight;

    if (node->isExpanded) {
        for (int i = 0; i < node->childCount; i++) {
            handleTaskTreeHover(node->children[i], x, mx, my);
        }
    }
}









static bool handleCommandClickTaskNode(TaskNode* node, int x, int mx, int my) {
    const int lineHeight = TASK_LINE_HEIGHT;
    const int expandWidth = 28;
    const int checkboxWidth = 28;

    int indent = node->depth * TASK_INDENT_WIDTH;
    int drawX = x + indent;
    int drawY = currentY;

    bool wasClicked = false;
    bool isHovered = (my >= drawY && my < drawY + lineHeight);

    if (isHovered) {
        hoveredTask = node;

        if (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_LEFT)) {
            clearAllTaskSelections();

            selectedTask = node;
            node->isSelected = true;
            wasClicked = true;

            int clickX = mx;

            if (nodeHasChildren(node) && isClickOnExpander(clickX, drawX)) {
                toggleTaskExpansion(node);
            } else if (isClickOnCheckbox(clickX, drawX, expandWidth, checkboxWidth)) {
                toggleTaskCompletion(node);
            }
        }
    }

    currentY += lineHeight;

    if (node->isExpanded) {
        for (int i = 0; i < node->childCount; i++) {
            if (handleCommandClickTaskNode(node->children[i], x, mx, my)) {
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

static bool isClickOnExpander(int clickX, int drawX) {
    return (clickX >= drawX && clickX <= drawX + 28);
}

static bool isClickOnCheckbox(int clickX, int drawX, int expandWidth, int checkboxWidth) {
    return (clickX >= drawX + expandWidth && clickX <= drawX + expandWidth + checkboxWidth);
}

static void toggleTaskExpansion(TaskNode* node) {
    node->isExpanded = !node->isExpanded;
}

static void toggleTaskCompletion(TaskNode* node) {
    node->completed = !node->completed;
}






 

static bool tryClickAddTaskButton(int x, int y, int mx, int my) {   
    SDL_Rect rect = { x, y, TASK_BUTTON_HEIGHT, TASK_BUTTON_HEIGHT };
    if (!pointInRect(mx, my, rect)) return false;
        
    handleCommandAddTask();
    return true;
}


static bool tryClickRemoveTaskButton(int x, int y, int mx, int my) {
    SDL_Rect rect = { x, y, TASK_BUTTON_HEIGHT, TASK_BUTTON_HEIGHT };
    if (!pointInRect(mx, my, rect)) return false;
    
    if (selectedTask) {
        handleCommandRemoveSelectedTask();
    }
    return true;  // Click is always consumed
}  



static int getTaskTreeStartY(UIPane* pane) {
    return pane->y + TASK_PANEL_TOP_PADDING + 2 * (TASK_BUTTON_HEIGHT + TASK_BUTTON_SPACING);
}


static void updateTaskTreeHover(int x, int startY, int mx, int my) {
    currentY = startY;
    for (int i = 0; i < taskRootCount; i++) {
        handleTaskTreeHover(taskRoots[i], x, mx, my);
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





void handleTaskMouseClick(UIPane* pane, SDL_Event* event) {
    int mx = event->button.x;
    int my = event->button.y;

    int x = pane->x + TASK_PANEL_LEFT_PADDING;
    int y = pane->y + TASK_PANEL_TOP_PADDING;

    if (tryClickAddTaskButton(mx, my, x, y)) return;
    y += TASK_BUTTON_HEIGHT + TASK_BUTTON_SPACING;

    if (tryClickRemoveTaskButton(mx, my, x, y)) return;
    y += TASK_BUTTON_HEIGHT + TASK_BUTTON_SPACING;

    handleClickTaskTree(mx, my, x, y);
}



static void handleClickTaskTree(int mx, int my, int x, int yStart) {
    TaskNode* prevSelected = selectedTask;
    currentY = yStart;
    bool clickedNode = false;

    for (int i = 0; i < taskRootCount; i++) {
        if (handleCommandClickTaskNode(taskRoots[i], x, mx, my)) {
            clickedNode = true;
        }
    }

    // If the selection changed or nothing was clicked, update state
    if (!clickedNode || selectedTask != prevSelected) {
        selectedTask = clickedNode ? selectedTask : NULL;
    }
}




static void handleCommandAddTask(void) {
    TaskNode* parent = resolveAddTarget(selectedTask);


    TaskNode* newTask = createTaskNode("New Task", false, parent);

    if (parent) {
        addTaskChild(parent, newTask);
    } else {
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
    beginRenamingSelectedTask();
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

