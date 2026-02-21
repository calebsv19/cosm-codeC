#ifndef TOOL_TASKS_H
#define TOOL_TASKS_H

#include "ide/Panes/PaneInfo/pane.h"
#include "core/CommandBus/command_metadata.h"

#include <SDL2/SDL.h>
#include <stdbool.h>

#define MAX_TASK_ROOTS 16
#define MAX_CHILDREN_PER_TASK 32

// Layout Constants (used in rendering and events)
#define TASK_PANEL_LEFT_PADDING 12
#define TASK_PANEL_TOP_PADDING 32
#define TASK_BUTTON_HEIGHT 24
#define TASK_BUTTON_SPACING 8
#define TASK_TREE_TOP_GAP 4
#define TASK_LINE_HEIGHT 22
#define TASK_INDENT_WIDTH 20
#define TASK_BUTTON_REMOVE_OFFSET 10

extern bool includeNestLabel;



typedef struct TaskNode {
    char label[128];
    bool completed;
    bool isExpanded;
    bool isGroup;
    int depth;

    struct TaskNode* parent;
    struct TaskNode* children[MAX_CHILDREN_PER_TASK];
    int childCount;

    bool isSelected;
    bool isHovered;

    bool isEditing;
} TaskNode;

// Global state
extern TaskNode** taskRoots;
extern int taskRootCount;

extern TaskNode* selectedTask;
extern TaskNode* hoveredTask;

// Expose for rendering
extern TaskNode* editingTask;
extern char taskEditingBuffer[128];




// Lifecycle
void initTaskPanel();
void handleTaskMouseMotion(UIPane* pane, SDL_Event* event);
void handleTaskLeftClick(UIPane* pane, SDL_Event* event);
void handleTaskRightClick(UIPane* pane, SDL_Event* event);



// Commands
void taskPanelAddTask(UIPane* pane);
void taskPanelRenameTask(UIPane* pane);
void taskPanelDeleteTask(UIPane* pane);
void taskPanelMoveTaskUp(UIPane* pane);
void taskPanelMoveTaskDown(UIPane* pane);




// Tree management
TaskNode* createTaskNode(const char* label, bool isGroup, TaskNode* parent);
void addTaskChild(TaskNode* parent, TaskNode* child);
void freeTaskTree(TaskNode* node);

#endif
