#ifndef IDEBRIDGE_FILE_UTILS_H
#define IDEBRIDGE_FILE_UTILS_H

#include <stdbool.h>

#include <json-c/json.h>

void idebridge_parse_files_csv(const char* csv, json_object* files_arr);
bool idebridge_read_file_text(const char* path, char** out_text);
unsigned long long idebridge_fnv1a64_file(const char* path, bool* ok_out);
int idebridge_collect_diff_paths(const char* diff_text, char paths[][1024], int max_paths);

#endif
