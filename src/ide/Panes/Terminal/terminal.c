#include "terminal.h"
#include "engine/Render/render_pipeline.h"   // drawText, drawClippedText
#include <stdio.h>
#include <string.h>

static char terminalLines[MAX_TERMINAL_LINES][MAX_TERMINAL_LINE_LENGTH];
static int terminalLineCount = 0;

void initTerminal() {
    terminalLineCount = 0;
    memset(terminalLines, 0, sizeof(terminalLines));
}

void printToTerminal(const char* text) {
    if (terminalLineCount >= MAX_TERMINAL_LINES) {
        // Shift lines up
        for (int i = 1; i < MAX_TERMINAL_LINES; i++) {
            strncpy(terminalLines[i - 1], terminalLines[i], MAX_TERMINAL_LINE_LENGTH);
        }
        terminalLineCount--;
    }

    strncpy(terminalLines[terminalLineCount], text, MAX_TERMINAL_LINE_LENGTH - 1);
    terminalLineCount++;
}

void clearTerminal() {
    terminalLineCount = 0;
    memset(terminalLines, 0, sizeof(terminalLines));
}

const char** getTerminalBuffer() {
    static const char* buffer[MAX_TERMINAL_LINES];
    for (int i = 0; i < terminalLineCount; i++) {
        buffer[i] = terminalLines[i];
    }
    return buffer;
}

int getTerminalLineCount() {
    return terminalLineCount;
}

