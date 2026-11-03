#include "save_publication.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Follow existing file links, but never split a hard-linked source file. */
bool publishEditorSnapshot(const char* path, const char* contents, size_t length) {
    struct stat original, current;
    char* resolved = realpath(path, NULL);
    bool exists = resolved != NULL;
    if (!exists && (errno != ENOENT || lstat(path, &current) == 0 || errno != ENOENT))
        return false;
    const char* target = exists ? resolved : path;
    bool ok = false;
    if (exists && (stat(target, &original) || !S_ISREG(original.st_mode) ||
                   original.st_nlink != 1 || access(target, W_OK))) goto done;
    const char suffix[] = ".codec-save-XXXXXX";
    const char* slash = strrchr(target, '/');
    size_t prefix = slash ? (size_t)(slash + 1 - target) : 0;
    char* stage = malloc(prefix + sizeof(suffix));
    if (!stage) goto done;
    memcpy(stage, target, prefix);
    memcpy(stage + prefix, suffix, sizeof(suffix));
    int fd = mkstemp(stage);
    if (fd >= 0) {
        size_t offset = 0;
        while (offset < length) {
            ssize_t n = write(fd, contents + offset, length - offset);
            if (n < 0 && errno == EINTR) continue;
            if (n <= 0) break;
            offset += (size_t)n;
        }
        ok = offset == length && (!exists || fchmod(fd, original.st_mode & 0777) == 0) && fsync(fd) == 0;
        if (close(fd) != 0) ok = false;
        if (ok && exists) {
            ok = stat(path, &current) == 0 && current.st_dev == original.st_dev &&
                 current.st_ino == original.st_ino && current.st_nlink == 1 &&
                 rename(stage, target) == 0;
        } else if (ok) {
            ok = link(stage, target) == 0;
        }
        if (!ok || !exists) unlink(stage);
    }
    free(stage);
done:
    free(resolved);
    return ok;
}
