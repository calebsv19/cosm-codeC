#ifndef TOOL_PROJECT_RUN_TARGET_HELPERS_H
#define TOOL_PROJECT_RUN_TARGET_HELPERS_H

#include <stddef.h>

#include "app/GlobalInfo/project.h"

void project_run_target_clear(char* run_target_path, size_t cap);
void project_run_target_update_from_entry(DirEntry* entry, char* run_target_path, size_t cap);

#endif // TOOL_PROJECT_RUN_TARGET_HELPERS_H
