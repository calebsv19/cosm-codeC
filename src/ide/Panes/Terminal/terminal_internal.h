#ifndef IDE_PANES_TERMINAL_INTERNAL_H
#define IDE_PANES_TERMINAL_INTERNAL_H

#include "terminal.h"
#include "core/Terminal/terminal_backend.h"
#include "ide/Panes/Terminal/terminal_journal.h"
#include "ide/UI/scroll_manager.h"

typedef struct {
    TerminalBackend* backend;
    size_t backendConsumed;
    bool backendExitNotified;
    TermGrid grid;
    int gridRows;
    int gridCols;
    int cellWidth;
    int cellHeight;
    int lastViewportW;
    int lastViewportH;
    int lastBackendRows;
    int lastBackendCols;
    PaneScrollState scrollState;
    bool scrollInitialized;
    SDL_Rect scrollTrack;
    SDL_Rect scrollThumb;
    bool followOutput;
    char name[64];
    int id;
    bool inUse;
    bool isBuild;
    bool isRun;
    TerminalVisibleBuffer visibleModel;
    TerminalScrollbackRing scrollbackModel;
    TerminalJournal journal;
} TerminalSession;

#define MAX_TERMINAL_SESSIONS 8

enum {
    TERMINAL_SCROLLBACK_EXTRA_ROWS_DEFAULT = 50000,
    TERMINAL_INITIAL_ROWS = 1024,
    TERMINAL_INITIAL_COLS = 120,
};

extern TerminalSession g_sessions[MAX_TERMINAL_SESSIONS];
extern int g_session_count;
extern int g_active_index;
extern int g_terminal_scrollback_extra_rows;
extern bool g_terminal_enable_alternate_screen;

TerminalSession* active_session(void);
int terminal_scrollback_extra_rows(void);
int terminal_session_content_rows(const TerminalSession* s);
int terminal_viewport_start_row(const TerminalSession* s);
void terminal_rebuild_session_model(TerminalSession* s, const char* reason);
void terminal_capture_journal_snapshot(TerminalSession* s, const char* reason);
void terminal_validate_invariants(const TerminalSession* s);
void terminal_ensure_session_scroll_state(TerminalSession* s);
void ensure_terminal_scroll_state(void);

void terminal_model_log(const char* fmt, ...);
void terminal_pipeline_log(const char* fmt, ...);
void terminal_feed_bytes(TerminalSession* s, const char* bytes, size_t len);
int terminal_create_task_session(const char* name, bool isBuild, bool isRun);
void terminal_jump_to_bottom(void);

#endif
