#ifndef TOOL_PROJECT_TREE_SNAPSHOT_HELPERS_H
#define TOOL_PROJECT_TREE_SNAPSHOT_HELPERS_H

#include <stdbool.h>
#include <stddef.h>

#include "app/GlobalInfo/project.h"

const char* project_tree_display_name(const DirEntry* entry);
bool project_tree_build_visible_snapshot(const DirEntry* root, char** out_text, size_t* out_len);

#endif // TOOL_PROJECT_TREE_SNAPSHOT_HELPERS_H
