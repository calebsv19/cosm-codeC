#ifndef TERMINAL_H
#define TERMINAL_H

#include <SDL2/SDL.h>
#include <stdbool.h>
#include "ide/Panes/Terminal/terminal_grid.h"

#define TERMINAL_LINE_HEIGHT 20
#define TERMINAL_PADDING 6

struct PaneScrollState;

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

struct PaneScrollState* terminal_get_scroll_state(void);
void terminal_set_scroll_track(const SDL_Rect* track, const SDL_Rect* thumb);
void terminal_get_scroll_track(SDL_Rect* track, SDL_Rect* thumb);
void terminal_set_follow_output(bool follow);
bool terminal_is_following_output(void);

// Input to the PTY shell (UTF-8 bytes; caller provides already-encoded data).
void terminal_send_text(const char* text, size_t len);

// Backend lifecycle (PTY shell). Rows/cols are optional hints; pass 0 for defaults.
bool terminal_spawn_shell(const char* start_dir, int rows, int cols);
void terminal_shutdown_shell(void);
void terminal_tick_backend(void);  // Non-blocking pump; call from frame loop
void terminal_resize_grid_for_pane(int width_px, int height_px);

// Expose grid and cell metrics for rendering.
extern TermGrid g_termGrid;
extern int g_cellWidth;
extern int g_cellHeight;

#endif
