#ifndef FILE_OPS_H
#define FILE_OPS_H

#include <stdbool.h>
#include <stddef.h>

bool renameFileOnDisk(const char* oldPath, const char* newPath);
bool writeTextFile(const char* path, const char* text, size_t len);
bool readTextFile(const char* path, char** outText, size_t* outLen);

#endif
