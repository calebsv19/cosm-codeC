#include "test_fixture_utils.h"

#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

int ide_test_fixture_root(char* out, size_t out_cap, const char* name) {
    if (!out || out_cap == 0 || !name || !name[0]) return 0;
    int written = snprintf(out, out_cap, "/tmp/%s_%ld", name, (long)getpid());
    return written > 0 && (size_t)written < out_cap;
}

int ide_test_ensure_dir(const char* path) {
    if (!path || !path[0]) return 0;
    if (mkdir(path, 0755) == 0) return 1;
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

int ide_test_prepare_workspace(char* root, size_t root_cap, const char* name) {
    if (!ide_test_fixture_root(root, root_cap, name)) return 0;
    if (!ide_test_ensure_dir(root)) return 0;
    char path[1024];
    int written = snprintf(path, sizeof(path), "%s/ide_files", root);
    if (written <= 0 || (size_t)written >= sizeof(path)) return 0;
    return ide_test_ensure_dir(path);
}

int ide_test_write_text_file(const char* path, const char* text) {
    FILE* f = fopen(path, "w");
    if (!f) return 0;
    int ok = fputs(text ? text : "", f) >= 0;
    return fclose(f) == 0 && ok;
}

int ide_test_write_sparse_file(const char* path, long long size) {
    if (!path || size <= 0) return 0;
    FILE* f = fopen(path, "w");
    if (!f) return 0;
    int ok = 1;
    if (fseek(f, size - 1, SEEK_SET) != 0) {
        ok = 0;
    } else if (fputc('\0', f) == EOF) {
        ok = 0;
    }
    return fclose(f) == 0 && ok;
}

int ide_test_file_exists(const char* path) {
    struct stat st;
    return path && stat(path, &st) == 0 && S_ISREG(st.st_mode);
}
