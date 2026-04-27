#include "ide/Panes/Terminal/terminal.h"
#include "ide/Panes/Terminal/terminal_selection_helpers.h"
#include "core/Clipboard/clipboard.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    bool selecting;
    bool hasSelection;
    int anchorLine;
    int anchorCol;
    int cursorLine;
    int cursorCol;
} TerminalSelectionState;

static TerminalSelectionState g_selection = {0};

static int terminal_encode_codepoint_utf8(uint32_t cp, char out[4]) {
    if (cp <= 0x7Fu) {
        out[0] = (char)cp;
        return 1;
    }
    if (cp <= 0x7FFu) {
        out[0] = (char)(0xC0u | ((cp >> 6) & 0x1Fu));
        out[1] = (char)(0x80u | (cp & 0x3Fu));
        return 2;
    }
    if (cp <= 0xFFFFu) {
        out[0] = (char)(0xE0u | ((cp >> 12) & 0x0Fu));
        out[1] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
        out[2] = (char)(0x80u | (cp & 0x3Fu));
        return 3;
    }
    if (cp <= 0x10FFFFu) {
        out[0] = (char)(0xF0u | ((cp >> 18) & 0x07u));
        out[1] = (char)(0x80u | ((cp >> 12) & 0x3Fu));
        out[2] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
        out[3] = (char)(0x80u | (cp & 0x3Fu));
        return 4;
    }
    out[0] = '?';
    return 1;
}

int terminal_line_length(int row, bool trim_trailing) {
    (void)trim_trailing;
    TermGrid* grid = terminal_active_grid();
    if (!grid || grid->cols <= 0) return 0;
    TerminalProjectionRow pr = {0};
    if (!terminal_projection_get_row(row, &pr)) return 0;
    int len = grid->cols;
    while (len > 0) {
        const TermCell* cell = terminal_projection_rowcol_to_cell(row, len - 1);
        uint32_t ch = cell ? cell->ch : (uint32_t)' ';
        if (ch == 0u) ch = (uint32_t)' ';
        if (ch != (uint32_t)' ') break;
        len--;
    }
    return len;
}

int terminal_line_to_string(int row, char* out, int cap, bool trim_trailing) {
    if (!out || cap <= 0) return 0;
    TermGrid* grid = terminal_active_grid();
    if (!grid || grid->cols <= 0) {
        out[0] = '\0';
        return 0;
    }
    TerminalProjectionRow pr = {0};
    if (!terminal_projection_get_row(row, &pr)) {
        out[0] = '\0';
        return 0;
    }

    int limitCols = grid->cols;
    if (trim_trailing) {
        limitCols = terminal_line_length(row, true);
    }

    int outLen = 0;
    for (int c = 0; c < limitCols; ++c) {
        const TermCell* cell = terminal_projection_rowcol_to_cell(row, c);
        uint32_t cp = cell ? cell->ch : (uint32_t)' ';
        if (cp == 0u) cp = (uint32_t)' ';
        if (cp < 0x20u || cp == 0x7Fu) cp = (uint32_t)' ';
        char enc[4];
        int encLen = terminal_encode_codepoint_utf8(cp, enc);
        if (outLen + encLen >= cap) break;
        for (int i = 0; i < encLen; ++i) {
            out[outLen++] = enc[i];
        }
    }
    out[outLen] = '\0';
    return outLen;
}

int terminal_last_used_row(void) {
    return terminal_projection_row_count() - 1;
}

int terminal_content_rows(void) {
    return terminal_projection_row_count();
}

static void clamp_selection_position(int* line, int* column) {
    int totalRows = terminal_projection_row_count();
    if (totalRows < 1) totalRows = 1;
    if (*line < 0) *line = 0;
    if (*line >= totalRows) *line = totalRows - 1;
    if (*line < 0) {
        *line = 0;
        *column = 0;
        return;
    }
    int len = terminal_line_length(*line, true);
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

void terminal_begin_selection(int line, int column) {
    TermGrid* grid = terminal_active_grid();
    if (!grid || !grid->cells || terminal_projection_row_count() <= 0) {
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
    TermGrid* grid = terminal_active_grid();
    if (!grid || !g_selection.selecting || terminal_projection_row_count() <= 0) return;
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
    TermGrid* grid = terminal_active_grid();
    if (!grid || !grid->cells || terminal_projection_row_count() <= 0) return false;
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
        int lineLen = terminal_line_length(line, true);
        int from = (line == startLine) ? startCol : 0;
        int to = (line == endLine) ? endCol : lineLen;
        if (from < 0) from = 0;
        if (to < from) to = from;
        if (to > lineLen) to = lineLen;
        total += (size_t)(to - from);
        if (line != endLine) total++;
    }

    char* buffer = (char*)malloc(total + 1);
    if (!buffer) return false;

    size_t offset = 0;
    for (int line = startLine; line <= endLine; ++line) {
        int lineLen = terminal_line_length(line, true);
        int from = (line == startLine) ? startCol : 0;
        int to = (line == endLine) ? endCol : lineLen;
        if (from < 0) from = 0;
        if (to < from) to = from;
        if (to > lineLen) to = lineLen;
        int chunk = to - from;
        if (chunk > 0) {
            char* temp = (char*)malloc((size_t)lineLen + 1);
            if (!temp) {
                free(buffer);
                return false;
            }
            terminal_line_to_string(line, temp, lineLen + 1, true);
            memcpy(buffer + offset, temp + from, (size_t)chunk);
            offset += (size_t)chunk;
            free(temp);
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

void terminal_selection_validate_projection_rows(int projected_rows) {
    if (projected_rows <= 0) {
        terminal_clear_selection();
        return;
    }
    if (!g_selection.hasSelection) return;
    if (g_selection.anchorLine < 0 || g_selection.anchorLine >= projected_rows ||
        g_selection.cursorLine < 0 || g_selection.cursorLine >= projected_rows) {
        terminal_clear_selection();
    }
}
