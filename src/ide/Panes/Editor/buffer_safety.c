#include "buffer_safety.h"
#include <stdlib.h>
#include <string.h>

void enforceNonEmptyBuffer(EditorBuffer* buffer) {
    if (!buffer) return;

    if (buffer->lineCount <= 0 || buffer->lines == NULL) {
        buffer->lines = malloc(sizeof(char*));
        buffer->lines[0] = strdup("");
        buffer->lineCount = 1;
        buffer->capacity = 1;
    }
}

