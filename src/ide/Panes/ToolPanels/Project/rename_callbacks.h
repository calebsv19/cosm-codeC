#ifndef RENAME_CALLBACKS_H
#define RENAME_CALLBACKS_H

#include <stdbool.h>
#include "app/GlobalInfo/project.h"

void handleProjectFileRenameCallback(const char* oldName, const char* newName, void* context);
bool isRenameValid(const char* newName, DirEntry* entry);

#endif // RENAME_CALLBACKS_H

