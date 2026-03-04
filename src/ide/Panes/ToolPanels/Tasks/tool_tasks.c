#include "tool_tasks.h"
#include "ide/Panes/ToolPanels/tool_panel_adapter.h"
#include "engine/Render/render_pipeline.h"
#include "engine/Render/render_text_helpers.h"
#include "core/InputManager/UserInput/rename_flow.h"
#include "ide/UI/row_surface.h"
#include "ide/UI/row_activation.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct {
    TaskNode** task_roots;
    int task_root_count;
    TaskNode* selected_task;
    TaskNode* hovered_task;
    bool include_nest_label;
    TaskNode* editing_task;
    char task_editing_buffer[128];
    PaneScrollState scroll;
    bool scroll_init;
    SDL_Rect scroll_track;
    SDL_Rect scroll_thumb;
    UIPanelTaggedRect control_hit_storage[2];
    UIPanelTaggedRectList control_hits;
} TaskPanelState;

static TaskPanelState g_taskPanelBootstrapState = {0};
static bool g_taskPanelBootstrapInitialized = false;

static void task_panel_init_state(void* ptr) {
    TaskPanelState* state = (TaskPanelState*)ptr;
    if (!state) return;
    memset(state, 0, sizeof(*state));
    state->control_hits.items = state->control_hit_storage;
    state->control_hits.capacity =
        (int)(sizeof(state->control_hit_storage) / sizeof(state->control_hit_storage[0]));
}

void freeTaskTree(TaskNode* node);

static void task_panel_destroy_state(void* ptr) {
    TaskPanelState* state = (TaskPanelState*)ptr;
    if (!state) return;
    for (int i = 0; i < state->task_root_count; ++i) {
        if (state->task_roots && state->task_roots[i]) {
            freeTaskTree(state->task_roots[i]);
        }
    }
    free(state->task_roots);
    free(state);
}

static TaskPanelState* task_panel_state(void) {
    return (TaskPanelState*)tool_panel_resolve_state_slot(
        TOOL_PANEL_STATE_SLOT_TASKS,
        sizeof(TaskPanelState),
        task_panel_init_state,
        task_panel_destroy_state,
        &g_taskPanelBootstrapState,
        &g_taskPanelBootstrapInitialized
    );
}

TaskNode*** task_panel_roots_ptr(void) { return &task_panel_state()->task_roots; }
int* task_panel_root_count_ptr(void) { return &task_panel_state()->task_root_count; }
TaskNode** task_panel_selected_ptr(void) { return &task_panel_state()->selected_task; }
TaskNode** task_panel_hovered_ptr(void) { return &task_panel_state()->hovered_task; }
TaskNode** task_panel_editing_task_ptr(void) { return &task_panel_state()->editing_task; }
char* task_panel_editing_buffer(void) { return task_panel_state()->task_editing_buffer; }
bool* task_panel_include_nest_label_ptr(void) { return &task_panel_state()->include_nest_label; }
PaneScrollState* task_panel_scroll_state(void) { return &task_panel_state()->scroll; }
bool* task_panel_scroll_initialized_ptr(void) { return &task_panel_state()->scroll_init; }
SDL_Rect task_panel_scroll_track_rect(void) { return task_panel_state()->scroll_track; }
SDL_Rect task_panel_scroll_thumb_rect(void) { return task_panel_state()->scroll_thumb; }
void task_panel_set_scroll_rects(SDL_Rect track, SDL_Rect thumb) {
    task_panel_state()->scroll_track = track;
    task_panel_state()->scroll_thumb = thumb;
}
UIPanelTaggedRectList* task_panel_control_hits(void) { return &task_panel_state()->control_hits; }

// Used during recursive traversal
static int currentY = 0;



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

static void clearTaskNodeUIState(TaskNode* node, bool clearSelection);





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



void task_build_row_layout(const TaskNode* node,
                           const UIPane* pane,
                           int panelStartX,
                           int rowY,
                           UITaskRowLayout* outLayout) {
    if (!node || !outLayout) return;

    memset(outLayout, 0, sizeof(*outLayout));

    int indent = node->depth * TASK_INDENT_WIDTH;
    int drawX = panelStartX + indent;
    int drawY = rowY;
    bool hasExpandIcon = (node->isGroup || node->childCount > 0);
    int usedExpandWidth = (hasExpandIcon && includeNestLabel) ? TASK_EXPAND_WIDTH : 0;
    int labelWidth = getTextWidth(node->label);
    int totalWidth = usedExpandWidth + TASK_CHECKBOX_WIDTH + labelWidth + TASK_ROW_SPACING * 2;
    int boxX = drawX + TASK_EXPAND_WIDTH - usedExpandWidth - TASK_ROW_SPACING;

    UIRowSurfaceLayout rowSurface = ui_row_surface_layout_from_content(boxX,
                                                                       drawY,
                                                                       totalWidth,
                                                                       TASK_LINE_HEIGHT,
                                                                       TASK_ROW_BOX_PAD_X,
                                                                       TASK_ROW_BOX_PAD_Y);

    outLayout->drawX = drawX;
    outLayout->drawY = drawY;
    outLayout->labelX = drawX + TASK_EXPAND_WIDTH + TASK_CHECKBOX_WIDTH;
    outLayout->hasExpandIcon = hasExpandIcon;
    outLayout->expandRect = (SDL_Rect){ drawX - 7, drawY - 1, TASK_EXPAND_WIDTH, TASK_LINE_HEIGHT };
    outLayout->checkRect = (SDL_Rect){ drawX + TASK_EXPAND_WIDTH - 5, drawY - 1, TASK_CHECKBOX_WIDTH, TASK_LINE_HEIGHT };
    outLayout->fullRowRect = rowSurface.bounds;

    if (node->label[0] == '\0') {
        outLayout->labelRect = (SDL_Rect){ panelStartX, drawY - 1, pane ? pane->w : totalWidth, TASK_LINE_HEIGHT };
    } else {
        outLayout->labelRect = rowSurface.bounds;
    }
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

typedef struct UITaskHitResult {
    TaskNode* node;
    UITaskRowLayout row;
} UITaskHitResult;

typedef struct TaskRowActivationState {
    TaskNode* node;
} TaskRowActivationState;

static bool task_find_hit_node_recursive(TaskNode* node,
                                         const UIPane* pane,
                                         int panelStartX,
                                         int mouseX,
                                         int mouseY,
                                         UITaskHitResult* outHit) {
    if (!node) return false;

    UITaskRowLayout row = {0};
    task_build_row_layout(node, pane, panelStartX, currentY, &row);
    bool hit = ui_panel_rect_contains(&row.labelRect, mouseX, mouseY) ||
               (row.hasExpandIcon && ui_panel_rect_contains(&row.expandRect, mouseX, mouseY)) ||
               ui_panel_rect_contains(&row.checkRect, mouseX, mouseY);

    currentY += TASK_LINE_HEIGHT;

    if (hit) {
        if (outHit) {
            outHit->node = node;
            outHit->row = row;
        }
        return true;
    }

    if (node->isExpanded) {
        for (int i = 0; i < node->childCount; i++) {
            if (task_find_hit_node_recursive(node->children[i], pane, panelStartX, mouseX, mouseY, outHit)) {
                return true;
            }
        }
    }

    return false;
}

static bool task_find_hit_node(const UIPane* pane, int mouseX, int mouseY, UITaskHitResult* outHit) {
    if (!pane) return false;
    int treeX = pane->x + TASK_PANEL_LEFT_PADDING;
    currentY = getTaskTreeStartY((UIPane*)pane);

    for (int i = 0; i < taskRootCount; i++) {
        if (taskRoots[i] &&
            task_find_hit_node_recursive(taskRoots[i], pane, treeX, mouseX, mouseY, outHit)) {
            return true;
        }
    }
    return false;
}

static void task_row_select_single(void* user_data) {
    TaskRowActivationState* state = (TaskRowActivationState*)user_data;
    if (!state || !state->node) return;
    clearAllTaskSelections();
    selectedTask = state->node;
    state->node->isSelected = true;
}

static void task_row_prefix_action(void* user_data) {
    TaskRowActivationState* state = (TaskRowActivationState*)user_data;
    if (!state || !state->node) return;
    if (nodeHasChildren(state->node)) {
        toggleTaskExpansion(state->node);
    }
}

static bool task_activate_hit_row(const UITaskHitResult* hit, int mouseX, int mouseY) {
    if (!hit || !hit->node) return false;

    TaskRowActivationState activation = { .node = hit->node };
    bool inExpandBox = hit->row.hasExpandIcon && ui_panel_rect_contains(&hit->row.expandRect, mouseX, mouseY);
    bool inCheckBox = ui_panel_rect_contains(&hit->row.checkRect, mouseX, mouseY);

    if (inCheckBox) {
        task_row_select_single(&activation);
        toggleTaskCompletion(hit->node);
        return true;
    }

    (void)ui_row_activation_handle_primary(
        &(UIRowActivationContext){
            .double_click_tracker = NULL,
            .row_identity = (uintptr_t)hit->node,
            .double_click_ms = UI_DOUBLE_CLICK_MS_DEFAULT,
            .clicked_prefix = inExpandBox,
            .additive_modifier = false,
            .range_modifier = false,
            .wants_drag_start = false,
            .on_select_single = task_row_select_single,
            .on_prefix = task_row_prefix_action,
            .user_data = &activation
        });
    return true;
}






 

int getTaskTreeStartY(UIPane* pane) {
    int baseY = pane->y + TASK_PANEL_TOP_PADDING +
                2 * (TASK_BUTTON_HEIGHT + TASK_BUTTON_SPACING) +
                TASK_TREE_TOP_GAP;
    return baseY - (int)scroll_state_get_offset(task_panel_scroll_state());
}


void handleTaskMouseMotion(UIPane* pane, SDL_Event* event) {
    int mx = event->motion.x;
    int my = event->motion.y;

    handleTaskHoverAt(pane, mx, my);
}

void handleTaskHoverAt(UIPane* pane, int mx, int my) {
    if (!pane) return;

    clearAllTaskHoverStates();
    hoveredTask = NULL;

    UITaskHitResult hit = {0};
    if (task_find_hit_node(pane, mx, my, &hit)) {
        hoveredTask = hit.node;
        hit.node->isHovered = true;
    }
}

static bool task_handle_top_control_click(int mx, int my) {
    switch ((TaskTopControlId)ui_panel_tagged_rect_list_hit_test(task_panel_control_hits(), mx, my)) {
        case TASK_TOP_CONTROL_ADD:
            handleCommandAddTask();
            return true;
        case TASK_TOP_CONTROL_REMOVE:
            if (selectedTask) {
                handleCommandRemoveSelectedTask();
            }
            return true;
        case TASK_TOP_CONTROL_NONE:
        default:
            return false;
    }
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

    if (task_handle_top_control_click(mx, my)) return;

    UITaskHitResult hit = {0};
    if (!task_find_hit_node(pane, mx, my, &hit)) {
        clearAllTaskSelections();
        selectedTask = NULL;
        hoveredTask = NULL;
        return;
    }

    hoveredTask = hit.node;
    (void)task_activate_hit_row(&hit, mx, my);
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
        beginRenameWithPrompt(
            "Rename Task:",
            "Name already exists",
            selectedTask->label,
            handleTaskRenameCallback,
            isTaskNameValid,
            selectedTask,
            false
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
