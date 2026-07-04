#include "idebridge_file_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void idebridge_parse_files_csv(const char* csv, json_object* files_arr) {
    if (!csv || !*csv || !files_arr) return;
    char buf[2048];
    snprintf(buf, sizeof(buf), "%s", csv);
    char* save = NULL;
    for (char* tok = strtok_r(buf, ",", &save); tok; tok = strtok_r(NULL, ",", &save)) {
        if (*tok) json_object_array_add(files_arr, json_object_new_string(tok));
    }
}

bool idebridge_read_file_text(const char* path, char** out_text) {
    if (!path || !*path || !out_text) return false;
    *out_text = NULL;
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len < 0 || len > (64 * 1024 * 1024)) {
        fclose(f);
        return false;
    }
    char* buf = (char*)malloc((size_t)len + 1);
    if (!buf) {
        fclose(f);
        return false;
    }
    size_t n = fread(buf, 1, (size_t)len, f);
    fclose(f);
    buf[n] = '\0';
    *out_text = buf;
    return true;
}

unsigned long long idebridge_fnv1a64_file(const char* path, bool* ok_out) {
    unsigned long long hash = 1469598103934665603ULL;
    if (ok_out) *ok_out = false;
    FILE* f = fopen(path, "rb");
    if (!f) return hash;
    unsigned char buf[4096];
    for (;;) {
        size_t n = fread(buf, 1, sizeof(buf), f);
        if (n > 0) {
            for (size_t i = 0; i < n; ++i) {
                hash ^= (unsigned long long)buf[i];
                hash *= 1099511628211ULL;
            }
        }
        if (n < sizeof(buf)) break;
    }
    fclose(f);
    if (ok_out) *ok_out = true;
    return hash;
}

int idebridge_collect_diff_paths(const char* diff_text, char paths[][1024], int max_paths) {
    if (!diff_text || !*diff_text || !paths || max_paths <= 0) return 0;
    int count = 0;
    char* tmp = strdup(diff_text);
    if (!tmp) return 0;
    char* save = NULL;
    for (char* line = strtok_r(tmp, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        if (strncmp(line, "+++ ", 4) != 0) continue;
        const char* p = line + 4;
        while (*p == ' ' || *p == '\t') p++;
        if (strncmp(p, "a/", 2) == 0 || strncmp(p, "b/", 2) == 0) p += 2;
        if (strncmp(p, "/dev/null", 9) == 0) continue;
        char path[1024];
        size_t i = 0;
        while (p[i] && p[i] != '\t' && p[i] != ' ' && i < sizeof(path) - 1) {
            path[i] = p[i];
            i++;
        }
        path[i] = '\0';
        if (!path[0]) continue;
        bool exists = false;
        for (int k = 0; k < count; ++k) {
            if (strcmp(paths[k], path) == 0) {
                exists = true;
                break;
            }
        }
        if (!exists && count < max_paths) {
            snprintf(paths[count], 1024, "%s", path);
            count++;
        }
    }
    free(tmp);
    return count;
}
