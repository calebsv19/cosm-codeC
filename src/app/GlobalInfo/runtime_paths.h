#ifndef RUNTIME_PATHS_H
#define RUNTIME_PATHS_H

#include <stdbool.h>
#include <stddef.h>

bool ide_runtime_paths_init(const char* argv0);
const char* ide_runtime_resource_root(void);
const char* ide_runtime_executable_dir(void);

bool ide_runtime_join_resource_path(const char* relative_path,
                                    char* out_path,
                                    size_t out_cap);
bool ide_runtime_probe_resource_path(const char* relative_or_absolute_path,
                                     char* out_path,
                                     size_t out_cap);

#endif
