// undo_stack.h

#ifndef UNDO_STACK_H
#define UNDO_STACK_H

#include <stdbool.h>
#include <stddef.h>

#include "Editor/editor_view.h"

// Called when a user types or deletes and a snapshot should be stored
void pushUndoState(OpenFile* file);

// Undo the most recent edit operation
bool performUndo(OpenFile* file);

// Redo the most recently undone operation
bool performRedo(OpenFile* file);

// Clear the undo/redo history (e.g., when closing or reloading a file)
void clearUndoHistory(OpenFile* file);


void beginWordEdit(OpenFile* file, int row, int col);
void appendCharToWord(OpenFile* file, char ch);
void commitWordEdit(OpenFile* file);


#endif // UNDO_STACK_H

