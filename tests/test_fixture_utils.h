#ifndef IDE_TEST_FIXTURE_UTILS_H
#define IDE_TEST_FIXTURE_UTILS_H

#include <stddef.h>

int ide_test_fixture_root(char* out, size_t out_cap, const char* name);
int ide_test_ensure_dir(const char* path);
int ide_test_prepare_workspace(char* root, size_t root_cap, const char* name);
int ide_test_write_text_file(const char* path, const char* text);
int ide_test_write_sparse_file(const char* path, long long size);
int ide_test_file_exists(const char* path);

#endif
