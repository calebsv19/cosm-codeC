#include <stdint.h>
#include <limits.h>
#include "ide/Panes/Editor/editor_buffer.h"
#include "buffer_safety.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


char* getBufferSnapshot(EditorBuffer* buffer, size_t* outLength) {
    if (!outLength) return NULL;
    *outLength = 0;
    if (!buffer || buffer->lineCount == 0) {
        return strdup("");
    }

    // Total length = sum of all line lengths + 1 '\n' per line
    size_t totalLength = 0;
    for (int i = 0; i < buffer->lineCount; i++) {
        size_t len = strlen(buffer->lines[i]);
        if (len >= SIZE_MAX - totalLength - 1) return NULL;
        totalLength += len + 1;
    }

    char* snapshot = malloc(totalLength + 1); // +1 for final '\0'
    if (!snapshot) return NULL;
    char* cursor = snapshot;

    for (int i = 0; i < buffer->lineCount; i++) {
        size_t len = strlen(buffer->lines[i]);
        memcpy(cursor, buffer->lines[i], len);
        cursor += len;
        *cursor = '\n';
        cursor++;
    }

    *cursor = '\0';
    *outLength = cursor - snapshot;
    return snapshot;
}

bool loadSnapshotIntoBuffer(EditorBuffer* buffer, const char* data, size_t length) {
    if (!buffer || !data) return false;
    int count = 1;
    for (size_t i = 0; i < length; i++) {
        if (data[i] == '\n') {
            if (count == INT_MAX) return false;
            count++;
        }
    }
    char** lines = calloc((size_t)count, sizeof(*lines));
    if (!lines) return false;
    size_t start = 0;
    int row = 0;
    for (size_t end = 0; end <= length; end++) {
        if (end < length && data[end] != '\n') continue;
        lines[row] = malloc(end - start + 1);
        if (!lines[row]) {
            while (row) free(lines[--row]);
            free(lines);
            return false;
        }
        memcpy(lines[row], data + start, end - start);
        lines[row++][end - start] = 0;
        start = end + 1;
    }
    for (int i = 0; i < buffer->lineCount; i++) free(buffer->lines[i]);
    free(buffer->lines);
    buffer->lines = lines;
    buffer->capacity = buffer->lineCount = count;
    return true;
}

EditorBuffer* loadEditorBuffer(const char* filePath) {
    FILE* file = fopen(filePath, "r");
    if (!file) {
        perror("Failed to open file");
        return NULL;
    }

    EditorBuffer* buffer = malloc(sizeof(EditorBuffer));
    buffer->lines = malloc(sizeof(char*) * INITIAL_CAPACITY);
    buffer->capacity = INITIAL_CAPACITY;
    buffer->lineCount = 0;

    char line[MAX_LINE_LENGTH];

    while (fgets(line, sizeof(line), file)) {
        if (buffer->lineCount >= buffer->capacity) {
            buffer->capacity *= 2;
            buffer->lines = realloc(buffer->lines, sizeof(char*) * buffer->capacity);
        }

        // Remove newline character
        line[strcspn(line, "\n")] = '\0';

        buffer->lines[buffer->lineCount] = strdup(line);
        buffer->lineCount++;
    }

    fclose(file);

    enforceNonEmptyBuffer(buffer);

    printf("Buffer lines: %d\n", buffer->lineCount);
    return buffer;
}



void freeEditorBuffer(EditorBuffer* buffer) {
    for (int i = 0; i < buffer->lineCount; i++) {
        free(buffer->lines[i]);
    }
    free(buffer->lines);
    free(buffer);
}
