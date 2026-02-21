#include "core/FileIO/file_ops.h"

#include "core_io.h"

#include <stdlib.h>
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

bool writeTextFile(const char* path, const char* text, size_t len) {
    CoreResult r;
    if (!path || (!text && len > 0u)) return false;
    r = core_io_write_all(path, text, len);
    return r.code == CORE_OK;
}

bool readTextFile(const char* path, char** outText, size_t* outLen) {
    CoreBuffer buffer = {0};
    CoreResult r;
    char* text = NULL;
    if (!path || !outText) return false;

    r = core_io_read_all(path, &buffer);
    if (r.code != CORE_OK) return false;

    text = (char*)malloc(buffer.size + 1u);
    if (!text) {
        core_io_buffer_free(&buffer);
        return false;
    }
    if (buffer.size > 0u) {
        memcpy(text, buffer.data, buffer.size);
    }
    text[buffer.size] = '\0';
    core_io_buffer_free(&buffer);

    *outText = text;
    if (outLen) *outLen = buffer.size;
    return true;
}
