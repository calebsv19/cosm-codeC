#ifndef TERMINAL_H
#define TERMINAL_H

#include <SDL2/SDL.h>
#include <stdbool.h>

#define MAX_TERMINAL_LINES 512
#define MAX_TERMINAL_LINE_LENGTH 256

void initTerminal(void);
void printToTerminal(const char* text);           // Appends one line
void clearTerminal(void);                             // Wipes all content
const char** getTerminalBuffer(void);                 // Exposes read-only line array
int getTerminalLineCount(void);                       // Number of active lines

#endif

