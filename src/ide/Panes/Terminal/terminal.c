#include "terminal.h"
#include "engine/Render/render_pipeline.h"   // drawText, drawClippedText
#include "engine/Render/render_text_helpers.h"
#include "core/Clipboard/clipboard.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

static char terminalLines[MAX_TERMINAL_LINES][MAX_TERMINAL_LINE_LENGTH];
static int terminalLineCount = 0;

typedef struct {
    bool selecting;
    bool hasSelection;
    int anchorLine;
    int anchorCol;
    int cursorLine;
    int cursorCol;
} TerminalSelectionState;

static TerminalSelectionState g_selection = {0};

static void terminal_shift_lines_if_needed(void) {
    if (terminalLineCount < MAX_TERMINAL_LINES) return;
    for (int i = 1; i < MAX_TERMINAL_LINES; ++i) {
        memcpy(terminalLines[i - 1], terminalLines[i], MAX_TERMINAL_LINE_LENGTH);
    }
    terminalLineCount = MAX_TERMINAL_LINES - 1;
}

static void terminal_store_line(const char* start, size_t len) {
    if (!start) start = "";

    size_t copyLen = len;
    if (copyLen >= MAX_TERMINAL_LINE_LENGTH) {
        copyLen = MAX_TERMINAL_LINE_LENGTH - 1;
    }

    terminal_shift_lines_if_needed();

    char* dest = terminalLines[terminalLineCount];
    if (copyLen > 0) {
        memcpy(dest, start, copyLen);
    }
    dest[copyLen] = '\0';

    terminalLineCount++;
}

static void terminal_append_text(const char* text) {
    if (!text) return;

    const char* cursor = text;
    const char* lineStart = text;
    bool lastWasSeparator = false;

    while (*cursor) {
        if (*cursor == '\r' || *cursor == '\n') {
            terminal_store_line(lineStart, (size_t)(cursor - lineStart));
            lastWasSeparator = true;

            if (*cursor == '\r' && *(cursor + 1) == '\n') {
                cursor++;
            }
            cursor++;
            lineStart = cursor;
            continue;
        }
        lastWasSeparator = false;
        cursor++;
    }

    if (cursor != lineStart) {
        terminal_store_line(lineStart, (size_t)(cursor - lineStart));
        lastWasSeparator = false;
    } else if (lastWasSeparator) {
        terminal_store_line("", 0);
    }
}

static void clamp_selection_position(int* line, int* column) {
    if (*line < 0) *line = 0;
    if (*line >= terminalLineCount) *line = terminalLineCount - 1;
    if (*line < 0) {
        *line = 0;
        *column = 0;
        return;
    }

    const char* text = terminalLines[*line];
    int len = text ? (int)strlen(text) : 0;
    if (*column < 0) *column = 0;
    if (*column > len) *column = len;
}

static void normalize_selection(int* startLine, int* startCol, int* endLine, int* endCol) {
    int sLine = g_selection.anchorLine;
    int sCol = g_selection.anchorCol;
    int eLine = g_selection.cursorLine;
    int eCol = g_selection.cursorCol;

    if (sLine > eLine || (sLine == eLine && sCol > eCol)) {
        int tmpLine = sLine;
        int tmpCol = sCol;
        sLine = eLine;
        sCol = eCol;
        eLine = tmpLine;
        eCol = tmpCol;
    }

    *startLine = sLine;
    *startCol = sCol;
    *endLine = eLine;
    *endCol = eCol;
}

void initTerminal() {
    terminalLineCount = 0;
    memset(terminalLines, 0, sizeof(terminalLines));
    terminal_clear_selection();
}

void printToTerminal(const char* text) {
    terminal_append_text(text);
    terminal_clear_selection();
}

void clearTerminal() {
    terminalLineCount = 0;
    memset(terminalLines, 0, sizeof(terminalLines));
    terminal_clear_selection();
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

void terminal_begin_selection(int line, int column) {
    if (terminalLineCount == 0) {
        terminal_clear_selection();
        return;
    }
    clamp_selection_position(&line, &column);
    g_selection.selecting = true;
    g_selection.hasSelection = false;
    g_selection.anchorLine = line;
    g_selection.anchorCol = column;
    g_selection.cursorLine = line;
    g_selection.cursorCol = column;
}

void terminal_update_selection(int line, int column) {
    if (!g_selection.selecting || terminalLineCount == 0) return;
    clamp_selection_position(&line, &column);
    g_selection.cursorLine = line;
    g_selection.cursorCol = column;
    g_selection.hasSelection =
        (g_selection.anchorLine != g_selection.cursorLine) ||
        (g_selection.anchorCol != g_selection.cursorCol);
}

void terminal_end_selection(void) {
    g_selection.selecting = false;
}

void terminal_clear_selection(void) {
    g_selection = (TerminalSelectionState){0};
}

bool terminal_get_selection_bounds(int* startLine, int* startCol, int* endLine, int* endCol) {
    if (!terminal_has_selection()) return false;
    int sLine, sCol, eLine, eCol;
    normalize_selection(&sLine, &sCol, &eLine, &eCol);
    clamp_selection_position(&sLine, &sCol);
    clamp_selection_position(&eLine, &eCol);
    *startLine = sLine;
    *startCol = sCol;
    *endLine = eLine;
    *endCol = eCol;
    return true;
}

bool terminal_has_selection(void) {
    if (!g_selection.hasSelection) return false;
    if (terminalLineCount == 0) return false;
    if (g_selection.anchorLine == g_selection.cursorLine &&
        g_selection.anchorCol == g_selection.cursorCol) {
        return false;
    }
    return true;
}

bool terminal_copy_selection_to_clipboard(void) {
    if (!terminal_has_selection()) return false;

    int startLine, startCol, endLine, endCol;
    normalize_selection(&startLine, &startCol, &endLine, &endCol);
    clamp_selection_position(&startLine, &startCol);
    clamp_selection_position(&endLine, &endCol);

    size_t total = 0;
    for (int line = startLine; line <= endLine; ++line) {
        const char* text = terminalLines[line];
        if (!text) text = "";
        int from = (line == startLine) ? startCol : 0;
        int to = (line == endLine) ? endCol : (int)strlen(text);
        if (from < 0) from = 0;
        if (to < from) to = from;
        total += (size_t)(to - from);
        if (line != endLine) total++;
    }

    char* buffer = (char*)malloc(total + 1);
    if (!buffer) return false;

    size_t offset = 0;
    for (int line = startLine; line <= endLine; ++line) {
        const char* text = terminalLines[line];
        if (!text) text = "";
        int from = (line == startLine) ? startCol : 0;
        int to = (line == endLine) ? endCol : (int)strlen(text);
        if (from < 0) from = 0;
        if (to < from) to = from;
        size_t chunk = (size_t)(to - from);
        if (chunk > 0) {
            memcpy(buffer + offset, text + from, chunk);
            offset += chunk;
        }
        if (line != endLine) {
            buffer[offset++] = '\n';
        }
    }
    buffer[offset] = '\0';

    bool success = clipboard_copy_text(buffer);
    free(buffer);
    return success;
}
