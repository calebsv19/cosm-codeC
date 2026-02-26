#ifndef RENAME_CALLBACKS_H
#define RENAME_CALLBACKS_H

#include <stdbool.h>
#include "app/GlobalInfo/project.h"

typedef struct {
    char originalPath[1024];
    char originalName[256];
} ProjectRenameContext;

void handleProjectFileRenameCallback(const char* oldName, const char* newName, void* context);
bool isRenameValid(const char* newName, void* context);

#endif // RENAME_CALLBACKS_H
