#ifndef EDITOR_H
#define EDITOR_H

#include <SDL2/SDL.h>
#include <stdbool.h>

#include "pane.h"
#include "editor_buffer.h"
#include "editor_state.h"

#define MAX_CUT_BUFFER 256
#define MAX_SELECTION_LENGTH 4096

// ========== Global State ==========

typedef struct {
    char* cutLines[MAX_CUT_BUFFER];
    int count;
} CutBuffer;

extern CutBuffer editorCutBuffer;
extern char selectionBuffer[MAX_SELECTION_LENGTH];

extern EditorBuffer* editorBuffer;
extern EditorState* editorState;


struct OpenFile;


// ========== PUBLIC COMMAND API ==========

void insertCharAtCursor(char ch);
void deleteCharAtCursor(void);
void cutSelection(struct EditorView* view);
void copySelection(struct EditorView* view);
void pasteClipboard(struct EditorView* view);
void selectAllText(struct EditorView* view);




// ============================================================
// ️  HIGH-LEVEL INPUT DISPATCH (InputManager layer)
// ============================================================

void forwardEditorEvent(struct UIPane* pane, SDL_Event* event);
bool handleEditorScrollWheel(struct UIPane* pane, SDL_Event* event);
bool handleEditorScrollbarEvent(struct UIPane* pane, SDL_Event* event);
void handleEditorKeyDown(SDL_Event* event, struct EditorView* view, struct UIPane* pane);
void handleEditorMouseClick(struct UIPane* pane, SDL_Event* event, struct EditorView* view);
bool handleEditorScrollbarThumbClick(SDL_Event* event, int mx, int my);
void handleEditorMouseDrag(struct UIPane* pane, SDL_Event* event, struct EditorView* view);
bool handleEditorScrollbarDrag(SDL_Event* event);
bool resetCursorPositionToMouse(struct UIPane* pane, int mouseX, int mouseY, EditorBuffer* buffer, EditorState* 
state);

// ============================================================
//  EDITOR COMMAND ROUTING (CommandBus target layer)
// ============================================================

void handleCommandMovement(SDL_Keycode key, EditorBuffer* buffer, EditorState* state, struct EditorView* view, int 
paneHeight);
void handleCommandAction(SDL_Keycode key, EditorBuffer* buffer, EditorState* state, struct EditorView* view, struct 
UIPane* pane);
bool handleCommandNavigation(SDL_Keycode key, EditorBuffer* buffer, EditorState* state, int paneHeight, bool 
shiftHeld);
bool handleCommandLineClipboard(SDL_Keycode key, EditorBuffer* buffer, EditorState* state);
bool handleCommandTextClipboard(SDL_Keycode key, EditorBuffer* buffer, EditorState* state);

// ============================================================
//  MODIFIER-BASED COMMAND HANDLERS (Shift / Alt / Char input)
// ============================================================

void handleCommandShiftAction(SDL_Keycode key, EditorBuffer* buffer, EditorState* state);
bool isSpecialShiftAction(SDL_Keycode key);
void handleCommandAltAction(SDL_Keycode key, EditorBuffer* buffer, EditorState* state);
void handleCommandCharacterInput(SDL_Event* event, EditorBuffer* buffer, EditorState* state);

// ============================================================
// 🛠️ EDITOR INTERNAL COMMAND HELPERS (non-dispatch calls)
// ============================================================

void handleReturnKey(EditorBuffer* buffer, EditorState* state);
void handleBackspaceKey(EditorBuffer* buffer, EditorState* state);
void handleArrowKeyPress(SDL_Keycode key, EditorBuffer* buffer, EditorState* state, int paneHeight);

// ============================================================
// 🔎 SELECTION + TEXT STRUCTURE HELPERS
// ============================================================

bool isCursorInSelection(EditorState* state);
bool isLineWhitespaceOnly(const char* line);
bool isLineInSelection(int row, int* startCol, int* endCol, EditorBuffer* buffer, EditorState* state);
int getVisibleEditorLineCount(struct UIPane* pane);

// ============================================================
// 📋 CLIPBOARD OPERATIONS
// ============================================================

void pushCutLine(const char* line);
const char* peekLastCutLine(void);
void clearCutBuffer(void);

#endif // EDITOR_H

