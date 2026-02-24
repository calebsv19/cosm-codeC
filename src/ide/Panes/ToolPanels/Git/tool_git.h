#ifndef TOOL_GIT_H
#define TOOL_GIT_H

#include "ide/Panes/PaneInfo/pane.h"
#include <SDL2/SDL.h>
#include <stdbool.h>

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
#define MAX_GIT_LOG_ENTRIES 128
#define GIT_LOG_LINE_MAX 512
#define GIT_PANEL_HEADER_HEIGHT 80

extern GitFileEntry gitFiles[MAX_GIT_ENTRIES];
extern int gitFileCount;
extern char currentGitBranch[64];

typedef struct {
    char hash[16];        // short hash
    char message[256];    // subject line
    char author[128];     // author name
    char date[64];        // commit date (short)
} GitLogEntry;

extern GitLogEntry gitLogEntries[MAX_GIT_LOG_ENTRIES];
extern int gitLogCount;

// Run git status and branch to populate current git state
void refreshGitStatus(void);
void refreshGitLog(int maxEntries);

// Polls git status summary and marks the Git panel dirty when repository state changes.
void pollGitStatusWatcher(void);
Uint32 gitStatusWatchIntervalMs(void);
void resetGitStatusWatcher(void);

// Git panel UI state helpers
void git_panel_set_add_all_rect(SDL_Rect rect);
void git_panel_set_commit_rect(SDL_Rect rect);
void git_panel_set_message_rect(SDL_Rect rect);
bool git_panel_point_in_add_all(int x, int y);
bool git_panel_point_in_commit(int x, int y);
bool git_panel_point_in_message(int x, int y);
void git_panel_set_message_focus(bool focused);
bool git_panel_is_message_focused(void);
const char* git_panel_get_message(void);
int git_panel_get_message_cursor(void);
void git_panel_set_message(const char* text);
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
bool git_stage_all_changes(void);
bool git_commit_with_message(void);

#endif // TOOL_GIT_H
