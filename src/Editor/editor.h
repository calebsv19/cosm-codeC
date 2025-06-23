#ifndef EDITOR_H
#define EDITOR_H

#include <SDL2/SDL.h>
#include <stdbool.h>

#include "PaneInfo/pane.h"
#include "Editor/editor_buffer.h"
#include "Editor/editor_state.h"
#include "Editor/editor_view.h"


#define MAX_CUT_BUFFER 256
#define MAX_SELECTION_LENGTH 4096
#define clamp(v, lo, hi) ((v) < (lo) ? (lo) : ((v) > (hi) ? (hi) : (v)))

typedef struct {
    char* cutLines[MAX_CUT_BUFFER];
    int count;
} CutBuffer;

// Global state
extern CutBuffer editorCutBuffer;
extern char selectionBuffer[MAX_SELECTION_LENGTH];
extern EditorBuffer* editorBuffer;
extern EditorState* editorState;

void insertCharAtCursor(char ch);
void deleteCharAtCursor(void);
void cutSelection(struct EditorView* view);
void copySelection(struct EditorView* view);
void pasteClipboard(struct EditorView* view);
void selectAllText(struct EditorView* view);

#endif
