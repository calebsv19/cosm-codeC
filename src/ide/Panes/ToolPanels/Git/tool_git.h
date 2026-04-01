#ifndef TOOL_GIT_H
#define TOOL_GIT_H

#include "ide/Panes/PaneInfo/pane.h"
#include "ide/UI/interaction_timing.h"
#include "ide/UI/panel_control_widgets.h"
#include "ide/UI/scroll_manager.h"
#include <SDL2/SDL.h>
#include <stdbool.h>

struct UITreeNode;

typedef enum {
    GIT_STATUS_MODIFIED,
    GIT_STATUS_ADDED,
    GIT_STATUS_DELETED,
    GIT_STATUS_UNTRACKED,
    GIT_STATUS_STAGED
} GitStatus;

typedef struct {
    char path[256];       // Relative file path, e.g. "src/main.c"
    GitStatus status;     // Git status enum
    bool staged;          // Explicit staging flag
} GitFileEntry;

#define MAX_GIT_ENTRIES 512
#define GIT_LOG_LINE_MAX 2048
#define GIT_PANEL_HEADER_HEIGHT 80

typedef struct {
    char hash[16];        // short hash
    char message[256];    // subject line
    char author[128];     // author name
    char date[64];        // commit date (short)
} GitLogEntry;

// Run git status and branch to populate current git state
void refreshGitStatus(void);
void refreshGitLog(int maxEntries);
const char* git_panel_branch_name(void);
int git_panel_file_count(void);
const GitFileEntry* git_panel_file_at(int index);
int git_panel_log_count(void);
const GitLogEntry* git_panel_log_at(int index);
bool git_panel_log_is_loading(void);
void git_panel_prepare_for_render(void);
void resetGitTree(void);
struct UITreeNode* git_panel_tree(void);
PaneScrollState* git_panel_scroll(void);
SDL_Rect* git_panel_scroll_track(void);
SDL_Rect* git_panel_scroll_thumb(void);
UIDoubleClickTracker* git_panel_tree_double_click_tracker(void);

// Polls git status summary and marks the Git panel dirty when repository state changes.
void pollGitStatusWatcher(void);
Uint32 gitStatusWatchIntervalMs(void);
void resetGitStatusWatcher(void);

// Git panel UI state helpers
void git_panel_set_top_strip_layout(UIPanelThreeSegmentStripLayout layout);
UIPanelThreeSegmentStripLayout git_panel_get_top_strip_layout(void);
void git_panel_set_message_focus(bool focused);
bool git_panel_is_message_focused(void);
const char* git_panel_get_message(void);
int git_panel_get_message_cursor(void);
void git_panel_set_message(const char* text);
bool git_panel_handle_message_text_input(const SDL_Event* event);
bool git_panel_handle_message_edit_key(SDL_Keycode key);
void git_panel_insert_text(const char* text);
void git_panel_backspace(void);
void git_panel_delete(void);
void git_panel_move_cursor_left(void);
void git_panel_move_cursor_right(void);
void git_panel_move_cursor_home(void);
void git_panel_move_cursor_end(void);
const char* git_panel_get_status_text(void);
int git_panel_content_top(const UIPane* pane);
int git_panel_tree_content_top(const UIPane* pane);
void git_panel_tree_viewport(const UIPane* pane, UIPane* out_pane);
bool git_stage_all_changes(void);
bool git_commit_with_message(void);

#endif // TOOL_GIT_H
