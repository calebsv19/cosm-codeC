#ifndef TERMINAL_H
#define TERMINAL_H

#include <SDL2/SDL.h>
#include <stdbool.h>

#define MAX_TERMINAL_LINES 512
#define MAX_TERMINAL_LINE_LENGTH 256
#define TERMINAL_LINE_HEIGHT 20
#define TERMINAL_PADDING 6

struct PaneScrollState;

void initTerminal(void);
void printToTerminal(const char* text);           // Appends one line
void clearTerminal(void);                             // Wipes all content
const char** getTerminalBuffer(void);                 // Exposes read-only line array
int getTerminalLineCount(void);                       // Number of active lines

void terminal_begin_selection(int line, int column);
void terminal_update_selection(int line, int column);
void terminal_end_selection(void);
void terminal_clear_selection(void);
bool terminal_get_selection_bounds(int* startLine, int* startCol, int* endLine, int* endCol);
bool terminal_has_selection(void);
bool terminal_copy_selection_to_clipboard(void);

struct PaneScrollState* terminal_get_scroll_state(void);
void terminal_set_scroll_track(const SDL_Rect* track, const SDL_Rect* thumb);
void terminal_get_scroll_track(SDL_Rect* track, SDL_Rect* thumb);
void terminal_set_follow_output(bool follow);
bool terminal_is_following_output(void);

#endif
