#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "app/GlobalInfo/runtime_paths.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static void clear_runtime_env(void) {
    unsetenv("IDE_RESOURCE_ROOT");
    unsetenv("IDE_RESOURCE_PATH_DEBUG");
}

static void ensure_dir(const char* path) {
    if (!path || !path[0]) return;
    if (mkdir(path, 0755) == 0) return;
    if (errno == EEXIST) return;
    perror("mkdir");
    assert(0 && "mkdir failed");
}

static void ensure_dir_recursive(const char* path) {
    char buf[PATH_MAX];
    size_t len;
    if (!path || !path[0]) return;
    strncpy(buf, path, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    len = strlen(buf);
    if (len == 0) return;
    if (buf[len - 1] == '/') {
        buf[len - 1] = '\0';
    }
    for (char* p = buf + 1; *p; ++p) {
        if (*p == '/') {
            *p = '\0';
            ensure_dir(buf);
            *p = '/';
        }
    }
    ensure_dir(buf);
}

static void create_file(const char* path) {
    FILE* f = fopen(path, "w");
    assert(f && "failed to create file");
    fputs("ok\n", f);
    fclose(f);
}

static void mkdtemp_or_die(char* path_template) {
    char* r = mkdtemp(path_template);
    assert(r && "mkdtemp failed");
    (void)r;
}

static void realpath_or_copy(const char* in, char* out, size_t out_cap) {
    char tmp[PATH_MAX];
    if (realpath(in, tmp)) {
        strncpy(out, tmp, out_cap - 1);
        out[out_cap - 1] = '\0';
        return;
    }
    strncpy(out, in, out_cap - 1);
    out[out_cap - 1] = '\0';
}

static void test_env_override_wins(void) {
    char tmp_template[] = "/tmp/ide_runtime_paths_env.XXXXXX";
    char expected[PATH_MAX];
    const char* resolved;

    clear_runtime_env();
    mkdtemp_or_die(tmp_template);
    ensure_dir_recursive(tmp_template);
    setenv("IDE_RESOURCE_ROOT", tmp_template, 1);

    assert(ide_runtime_paths_init(NULL));
    resolved = ide_runtime_resource_root();
    realpath_or_copy(tmp_template, expected, sizeof(expected));
    assert(strcmp(resolved, expected) == 0);
}

static void test_exe_parent_scan_finds_dev_root(void) {
    char tmp_template[] = "/tmp/ide_runtime_paths_scan.XXXXXX";
    char root_expected[PATH_MAX];
    char exe_path[PATH_MAX];
    const char* resolved;

    clear_runtime_env();
    mkdtemp_or_die(tmp_template);

    {
        char include_fonts[PATH_MAX];
        char bin_dir[PATH_MAX];
        snprintf(include_fonts, sizeof(include_fonts), "%s/include/fonts", tmp_template);
        snprintf(bin_dir, sizeof(bin_dir), "%s/bin", tmp_template);
        ensure_dir_recursive(include_fonts);
        ensure_dir_recursive(bin_dir);
        snprintf(exe_path, sizeof(exe_path), "%s/ide", bin_dir);
        create_file(exe_path);
    }

    assert(ide_runtime_paths_init(exe_path));
    resolved = ide_runtime_resource_root();
    realpath_or_copy(tmp_template, root_expected, sizeof(root_expected));
    assert(strcmp(resolved, root_expected) == 0);
}

static void test_probe_resource_path(void) {
    char tmp_template[] = "/tmp/ide_runtime_paths_probe.XXXXXX";
    char file_path[PATH_MAX];
    char found[PATH_MAX];

    clear_runtime_env();
    mkdtemp_or_die(tmp_template);
    ensure_dir_recursive(tmp_template);
    setenv("IDE_RESOURCE_ROOT", tmp_template, 1);

    snprintf(file_path, sizeof(file_path), "%s/include/fonts/runtime_probe.ttf", tmp_template);
    {
        char dir_path[PATH_MAX];
        strncpy(dir_path, file_path, sizeof(dir_path) - 1);
        dir_path[sizeof(dir_path) - 1] = '\0';
        char* slash = strrchr(dir_path, '/');
        assert(slash);
        *slash = '\0';
        ensure_dir_recursive(dir_path);
    }
    create_file(file_path);

    assert(ide_runtime_paths_init(NULL));
    assert(ide_runtime_probe_resource_path("include/fonts/runtime_probe.ttf", found, sizeof(found)));
    assert(access(found, R_OK) == 0);
    assert(!ide_runtime_probe_resource_path("include/fonts/does_not_exist.ttf", found, sizeof(found)));
}

int main(void) {
    test_env_override_wins();
    test_exe_parent_scan_finds_dev_root();
    test_probe_resource_path();
    clear_runtime_env();
    puts("runtime_paths_resolution_test: success");
    return 0;
}
