#ifndef BUFFER_SAFETY_H
#define BUFFER_SAFETY_H

#include "editor_buffer.h"

// Ensures that a buffer always has at least one line
void enforceNonEmptyBuffer(EditorBuffer* buffer);

#endif

