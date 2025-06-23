#include "Editor/editor_buffer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


char* getBufferSnapshot(EditorBuffer* buffer, size_t* outLength) {
    if (!buffer || buffer->lineCount == 0) {
        *outLength = 0;
        return strdup("");
    }

    // Total length = sum of all line lengths + 1 '\n' per line
    size_t totalLength = 0;
    for (int i = 0; i < buffer->lineCount; i++) {
        totalLength += strlen(buffer->lines[i]) + 1; // +1 for newline
    }

    char* snapshot = malloc(totalLength + 1); // +1 for final '\0'
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


void loadSnapshotIntoBuffer(EditorBuffer* buffer, const char* data, size_t length) {
    if (!buffer || !data || length == 0) return;

    // Free old lines
    for (int i = 0; i < buffer->lineCount; i++) {
        free(buffer->lines[i]);
    }
    free(buffer->lines);

    // Count lines first
    int lineCount = 1;
    for (size_t i = 0; i < length; i++) {
        if (data[i] == '\n') lineCount++;
    }

    buffer->lines = malloc(sizeof(char*) * lineCount);
    buffer->capacity = lineCount;
    buffer->lineCount = 0;

    const char* lineStart = data;
    for (size_t i = 0; i <= length; i++) {
        if (i == length || data[i] == '\n') {
            size_t lineLen = &data[i] - lineStart;
            char* line = malloc(lineLen + 1);
            memcpy(line, lineStart, lineLen);
            line[lineLen] = '\0';
            buffer->lines[buffer->lineCount++] = line;
            lineStart = &data[i + 1];
        }
    }
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
    printf("Buffer lines: %d", buffer->lineCount);
    return buffer;
}

void freeEditorBuffer(EditorBuffer* buffer) {
    for (int i = 0; i < buffer->lineCount; i++) {
        free(buffer->lines[i]);
    }
    free(buffer->lines);
    free(buffer);
}
