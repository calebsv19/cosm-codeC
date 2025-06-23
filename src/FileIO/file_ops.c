#include "FileIO/file_ops.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h> // for rename()

bool renameFileOnDisk(const char* oldPath, const char* newPath) {
    if (rename(oldPath, newPath) == 0) {
        printf("[FileOps] Renamed: %s → %s\n", oldPath, newPath);
        return true;
    } else {
        perror("[FileOps] rename failed");
        return false;
    }
}

