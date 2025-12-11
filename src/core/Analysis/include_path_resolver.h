#ifndef INCLUDE_PATH_RESOLVER_H
#define INCLUDE_PATH_RESOLVER_H

#include <stddef.h>

// Build a list of include search paths for the current project (mirrors Makefile -I usage).
// The returned array and all strings are heap-allocated; caller must free via free_include_paths.
// Returns the number of paths (0 if none).
size_t gather_include_paths(const char* project_root, char*** outPaths);

void free_include_paths(char** paths, size_t count);

#endif // INCLUDE_PATH_RESOLVER_H
