#ifndef TOOL_TASKS_H
#define TOOL_TASKS_H

#include "ide/Panes/PaneInfo/pane.h"
#include "core/CommandBus/command_metadata.h"
#include "ide/UI/panel_control_widgets.h"
#include "ide/UI/panel_metrics.h"
#include "ide/UI/scroll_manager.h"

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
#define TASK_LINE_HEIGHT IDE_UI_DENSE_ROW_HEIGHT
#define TASK_INDENT_WIDTH 20
#define TASK_EXPAND_WIDTH 28
#define TASK_CHECKBOX_WIDTH 28
#define TASK_ROW_SPACING 6
#define TASK_ROW_BOX_PAD_X 6
#define TASK_ROW_BOX_PAD_Y 1
#define TASK_BUTTON_REMOVE_OFFSET 10

bool* task_panel_include_nest_label_ptr(void);



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

typedef struct UITaskRowLayout {
    int drawX;
    int drawY;
    int labelX;
    bool hasExpandIcon;
    SDL_Rect expandRect;
    SDL_Rect checkRect;
    SDL_Rect labelRect;
    SDL_Rect fullRowRect;
} UITaskRowLayout;

typedef enum TaskTopControlId {
    TASK_TOP_CONTROL_NONE = 0,
    TASK_TOP_CONTROL_ADD = 1,
    TASK_TOP_CONTROL_REMOVE = 2
} TaskTopControlId;

// Global state
TaskNode*** task_panel_roots_ptr(void);
int* task_panel_root_count_ptr(void);
TaskNode** task_panel_selected_ptr(void);
TaskNode** task_panel_hovered_ptr(void);
TaskNode** task_panel_editing_task_ptr(void);
char* task_panel_editing_buffer(void);
PaneScrollState* task_panel_scroll_state(void);
bool* task_panel_scroll_initialized_ptr(void);
SDL_Rect task_panel_scroll_track_rect(void);
SDL_Rect task_panel_scroll_thumb_rect(void);
void task_panel_set_scroll_rects(SDL_Rect track, SDL_Rect thumb);
UIPanelTaggedRectList* task_panel_control_hits(void);

#define includeNestLabel (*task_panel_include_nest_label_ptr())
#define taskRoots (*task_panel_roots_ptr())
#define taskRootCount (*task_panel_root_count_ptr())
#define selectedTask (*task_panel_selected_ptr())
#define hoveredTask (*task_panel_hovered_ptr())
#define editingTask (*task_panel_editing_task_ptr())
#define taskEditingBuffer (task_panel_editing_buffer())




// Lifecycle
void initTaskPanel();
void handleTaskMouseMotion(UIPane* pane, SDL_Event* event);
void handleTaskHoverAt(UIPane* pane, int mx, int my);
void handleTaskLeftClick(UIPane* pane, SDL_Event* event);
void handleTaskRightClick(UIPane* pane, SDL_Event* event);
int getTaskTreeStartY(UIPane* pane);
void task_build_row_layout(const TaskNode* node,
                           const UIPane* pane,
                           int panelStartX,
                           int rowY,
                           UITaskRowLayout* outLayout);



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
