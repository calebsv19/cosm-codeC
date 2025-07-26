#ifndef EDITOR_BUFFER_H
#define EDITOR_BUFFER_H

#include <stddef.h>

typedef struct {
    char** lines;       // array of lines
    int lineCount;      // how many lines are loaded
    int capacity;       // how many lines allocated
} EditorBuffer;


#define INITIAL_CAPACITY 64
#define MAX_LINE_LENGTH 2048

// Snapshot utility for saving
char* getBufferSnapshot(EditorBuffer* buffer, size_t* outLength);

void loadSnapshotIntoBuffer(EditorBuffer* buffer, const char* data, size_t length);

EditorBuffer* loadEditorBuffer(const char* filePath);
    
void freeEditorBuffer(EditorBuffer* buffer);        
        
        

#endif

