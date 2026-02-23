#ifndef TERMINAL_H
#define TERMINAL_H

#include <SDL2/SDL.h>
#include <stdbool.h>
#include "ide/Panes/Terminal/terminal_grid.h"

#define TERMINAL_LINE_HEIGHT 20
#define TERMINAL_PADDING 6
#define TERMINAL_HEADER_HEIGHT 28

struct PaneScrollState;

typedef struct {
    const TermCell* cells;
    int rows;
    int cols;
    int cursor_row;
    int cursor_col;
    bool using_alternate;
} TerminalVisibleBuffer;

typedef struct {
    int row_count;
    int cap_rows;
} TerminalScrollbackRing;

typedef struct {
    int projected_row;
    int grid_row;
    bool from_scrollback;
} TerminalProjectionRow;

typedef struct {
    bool using_alternate;
    int cursor_row;
    int cursor_col;
    int viewport_rows;
    int viewport_cols;
    int scrollback_rows;
    int projected_rows;
} TerminalDebugStats;

void initTerminal(void);
void printToTerminal(const char* text);           // Appends text into the grid
void clearTerminal(void);                         // Wipes all content

void terminal_begin_selection(int line, int column);
void terminal_update_selection(int line, int column);
void terminal_end_selection(void);
void terminal_clear_selection(void);
bool terminal_get_selection_bounds(int* startLine, int* startCol, int* endLine, int* endCol);
bool terminal_has_selection(void);
bool terminal_copy_selection_to_clipboard(void);

// Helpers for UI hit-testing/copy:
int terminal_line_to_string(int row, char* out, int cap, bool trim_trailing);
int terminal_line_length(int row, bool trim_trailing);
int terminal_last_used_row(void);
int terminal_content_rows(void);
TermGrid* terminal_active_grid(void);
bool terminal_get_visible_buffer(TerminalVisibleBuffer* out);
bool terminal_get_scrollback_ring(TerminalScrollbackRing* out);
int terminal_projection_row_count(void);
bool terminal_projection_get_row(int index, TerminalProjectionRow* out_row);
const TermCell* terminal_projection_rowcol_to_cell(int row, int col);
bool terminal_projection_rowcol_to_grid(int row, int col, int* out_grid_row, int* out_grid_col);
bool terminal_get_debug_stats(TerminalDebugStats* out);
bool terminal_debug_overlay_enabled(void);
int terminal_cell_width(void);
int terminal_cell_height(void);
int terminal_session_count(void);
int terminal_active_index(void);
bool terminal_set_active(int index);
int terminal_create_interactive(const char* start_dir);
void terminal_close_interactive(int index);
bool terminal_session_info(int index, const char** name, bool* isBuild, bool* isRun);
bool terminal_close_active_interactive(void);
bool terminal_active_is_task(void);
bool terminal_set_name(int index, const char* name);
int terminal_find_task(bool isBuild, bool isRun);
bool terminal_activate_task(bool isBuild, bool isRun);
void terminal_append_to_session(int index, const char* text);
void terminal_clear_session(int index);
void terminal_set_tab_rect(int index, SDL_Rect rect);
int terminal_tab_hit(int x, int y);
bool terminal_plus_hit(int x, int y);
void terminal_reset_tab_rects(void);
void terminal_set_plus_rect(SDL_Rect rect);
void terminal_set_close_rect(SDL_Rect rect);
bool terminal_close_hit(int x, int y);

struct PaneScrollState* terminal_get_scroll_state(void);
void terminal_set_scroll_track(const SDL_Rect* track, const SDL_Rect* thumb);
void terminal_get_scroll_track(SDL_Rect* track, SDL_Rect* thumb);
void terminal_set_follow_output(bool follow);
bool terminal_is_following_output(void);
void terminal_set_safe_paste_enabled(bool enabled);
bool terminal_safe_paste_enabled(void);
void terminal_toggle_safe_paste_enabled(void);

// Input to the PTY shell (UTF-8 bytes; caller provides already-encoded data).
void terminal_send_text(const char* text, size_t len);
// Scaffold: route dropped file paths into terminal input (shell-escaped).
void terminal_handle_dropped_path(const char* path);

// Backend lifecycle (PTY shell). Rows/cols are optional hints; pass 0 for defaults.
bool terminal_spawn_shell(const char* start_dir, int rows, int cols);
void terminal_shutdown_shell(void);
bool terminal_tick_backend(void);  // Non-blocking pump; returns true if content changed
void terminal_resize_grid_for_pane(int width_px, int height_px);

#endif
