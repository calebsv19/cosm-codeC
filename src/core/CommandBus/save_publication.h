#ifndef IDE_SAVE_PUBLICATION_H
#define IDE_SAVE_PUBLICATION_H
#include <stdbool.h>
#include <stddef.h>
bool publishEditorSnapshot(const char* path, const char* contents, size_t length);
#endif
