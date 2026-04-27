#include "core/Ipc/ide_ipc_search_helpers.h"

#include <dirent.h>
#include <limits.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

typedef struct {
    char* file;
    int line;
    int col;
    char* excerpt;
} SearchMatch;

typedef struct {
    SearchMatch* items;
    size_t count;
    size_t cap;
} SearchMatchList;

static json_object* build_search_error_obj(const char* code,
                                           const char* message,
                                           const char* details) {
    json_object* err = json_object_new_object();
    json_object_object_add(err, "code", json_object_new_string(code ? code : "unknown"));
    json_object_object_add(err, "message", json_object_new_string(message ? message : "unknown error"));
    if (details && *details) {
        json_object_object_add(err, "details", json_object_new_string(details));
    }
    return err;
}

static void free_search_matches(SearchMatchList* list) {
    if (!list) return;
    for (size_t i = 0; i < list->count; ++i) {
        free(list->items[i].file);
        free(list->items[i].excerpt);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->cap = 0;
}

static bool append_search_match(SearchMatchList* list,
                                const char* file,
                                int line,
                                int col,
                                const char* excerpt) {
    if (!list || !file || !excerpt) return false;
    if (list->count >= list->cap) {
        size_t new_cap = list->cap ? list->cap * 2 : 64;
        SearchMatch* tmp = (SearchMatch*)realloc(list->items, new_cap * sizeof(SearchMatch));
        if (!tmp) return false;
        list->items = tmp;
        list->cap = new_cap;
    }
    SearchMatch* m = &list->items[list->count++];
    m->file = strdup(file);
    m->line = line;
    m->col = col;
    m->excerpt = strdup(excerpt);
    return (m->file != NULL && m->excerpt != NULL);
}

static int search_match_cmp(const void* a, const void* b) {
    const SearchMatch* ma = (const SearchMatch*)a;
    const SearchMatch* mb = (const SearchMatch*)b;
    if (!ma || !mb) return 0;
    int c = strcmp(ma->file ? ma->file : "", mb->file ? mb->file : "");
    if (c != 0) return c;
    if (ma->line != mb->line) return (ma->line < mb->line) ? -1 : 1;
    if (ma->col != mb->col) return (ma->col < mb->col) ? -1 : 1;
    return 0;
}

static bool should_skip_search_dir(const char* name) {
    return strcmp(name, ".") == 0 ||
           strcmp(name, "..") == 0 ||
           strcmp(name, ".git") == 0 ||
           strcmp(name, "build") == 0 ||
           strcmp(name, "ide_files") == 0;
}

static bool file_is_in_filter_list(const char* path, json_object* files_filter) {
    if (!files_filter || !json_object_is_type(files_filter, json_type_array)) return true;
    size_t n = json_object_array_length(files_filter);
    if (n == 0) return true;
    for (size_t i = 0; i < n; ++i) {
        json_object* item = json_object_array_get_idx(files_filter, i);
        if (!item || !json_object_is_type(item, json_type_string)) continue;
        const char* f = json_object_get_string(item);
        if (!f || !*f) continue;
        if (strcmp(path, f) == 0) return true;
        size_t flen = strlen(f);
        size_t plen = strlen(path);
        if (plen >= flen && strcmp(path + (plen - flen), f) == 0) return true;
    }
    return false;
}

static bool search_file_collect(const char* file_path,
                                const char* pattern,
                                bool regex_mode,
                                regex_t* compiled_re,
                                SearchMatchList* out,
                                int max_items) {
    FILE* f = fopen(file_path, "r");
    if (!f) return true;
    char line[4096];
    int line_no = 0;
    while (fgets(line, sizeof(line), f)) {
        line_no++;
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }

        if (regex_mode) {
            if (!compiled_re) continue;
            const char* cursor = line;
            int base_col = 1;
            while (*cursor) {
                regmatch_t m;
                int rc = regexec(compiled_re, cursor, 1, &m, 0);
                if (rc != 0) break;
                if (m.rm_so < 0) break;
                int col = base_col + (int)m.rm_so;
                if (!append_search_match(out, file_path, line_no, col, line)) {
                    fclose(f);
                    return false;
                }
                if (max_items >= 0 && (int)out->count >= max_items) {
                    fclose(f);
                    return true;
                }
                int advance = (int)m.rm_eo;
                if (advance <= 0) advance = 1;
                cursor += advance;
                base_col += advance;
            }
        } else {
            const char* cursor = line;
            while (cursor && *cursor) {
                const char* found = strstr(cursor, pattern);
                if (!found) break;
                int col = (int)(found - line) + 1;
                if (!append_search_match(out, file_path, line_no, col, line)) {
                    fclose(f);
                    return false;
                }
                if (max_items >= 0 && (int)out->count >= max_items) {
                    fclose(f);
                    return true;
                }
                cursor = found + 1;
            }
        }
    }
    fclose(f);
    return true;
}

static bool scan_search_dir(const char* root,
                            const char* pattern,
                            bool regex_mode,
                            regex_t* compiled_re,
                            json_object* files_filter,
                            SearchMatchList* out,
                            int max_items) {
    DIR* dir = opendir(root);
    if (!dir) return true;
    struct dirent* ent = NULL;
    char child[PATH_MAX];
    while ((ent = readdir(dir)) != NULL) {
        if (should_skip_search_dir(ent->d_name)) continue;
        snprintf(child, sizeof(child), "%s/%s", root, ent->d_name);
        struct stat st;
        if (stat(child, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            if (!scan_search_dir(child, pattern, regex_mode, compiled_re, files_filter, out, max_items)) {
                closedir(dir);
                return false;
            }
            if (max_items >= 0 && (int)out->count >= max_items) break;
        } else if (S_ISREG(st.st_mode)) {
            if (!file_is_in_filter_list(child, files_filter)) continue;
            if (!search_file_collect(child, pattern, regex_mode, compiled_re, out, max_items)) {
                closedir(dir);
                return false;
            }
            if (max_items >= 0 && (int)out->count >= max_items) break;
        }
    }
    closedir(dir);
    return true;
}

json_object* ide_ipc_build_search_result(json_object* args,
                                         const char* project_root,
                                         json_object** error_out) {
    const char* pattern = NULL;
    bool regex_mode = false;
    int max_items = 500;
    json_object* files_filter = NULL;
    if (error_out) *error_out = NULL;

    if (args) {
        json_object *jpat = NULL, *jregex = NULL, *jmax = NULL, *jfiles = NULL;
        if (json_object_object_get_ex(args, "pattern", &jpat) &&
            jpat && json_object_is_type(jpat, json_type_string)) {
            pattern = json_object_get_string(jpat);
        }
        if (json_object_object_get_ex(args, "regex", &jregex) &&
            jregex && json_object_is_type(jregex, json_type_boolean)) {
            regex_mode = json_object_get_boolean(jregex);
        }
        if (json_object_object_get_ex(args, "max", &jmax) &&
            jmax && json_object_is_type(jmax, json_type_int)) {
            max_items = json_object_get_int(jmax);
            if (max_items < 1) max_items = 1;
        }
        if (json_object_object_get_ex(args, "files", &jfiles) &&
            jfiles && json_object_is_type(jfiles, json_type_array)) {
            files_filter = jfiles;
        }
    }

    if (!pattern || !*pattern) {
        if (error_out) *error_out = build_search_error_obj("bad_request", "search requires args.pattern", NULL);
        return NULL;
    }
    if (!project_root || !*project_root) {
        if (error_out) *error_out = build_search_error_obj("bad_state", "Project root unavailable", NULL);
        return NULL;
    }

    regex_t re;
    regex_t* re_ptr = NULL;
    if (regex_mode) {
        if (regcomp(&re, pattern, REG_EXTENDED) != 0) {
            if (error_out) *error_out = build_search_error_obj("bad_regex", "Invalid regex pattern", pattern);
            return NULL;
        }
        re_ptr = &re;
    }

    SearchMatchList matches = {0};
    bool ok = scan_search_dir(project_root, pattern, regex_mode, re_ptr, files_filter, &matches, max_items);
    if (regex_mode) regfree(&re);
    if (!ok) {
        free_search_matches(&matches);
        if (error_out) *error_out = build_search_error_obj("search_failed", "Failed while scanning project files", NULL);
        return NULL;
    }

    if (matches.count > 1) {
        qsort(matches.items, matches.count, sizeof(SearchMatch), search_match_cmp);
    }

    json_object* result = json_object_new_object();
    json_object* arr = json_object_new_array();
    for (size_t i = 0; i < matches.count; ++i) {
        json_object* m = json_object_new_object();
        json_object_object_add(m, "file", json_object_new_string(matches.items[i].file ? matches.items[i].file : ""));
        json_object_object_add(m, "line", json_object_new_int(matches.items[i].line));
        json_object_object_add(m, "col", json_object_new_int(matches.items[i].col));
        json_object_object_add(m, "excerpt", json_object_new_string(matches.items[i].excerpt ? matches.items[i].excerpt : ""));
        json_object_array_add(arr, m);
    }
    json_object_object_add(result, "pattern", json_object_new_string(pattern));
    json_object_object_add(result, "regex", json_object_new_boolean(regex_mode));
    json_object_object_add(result, "max", json_object_new_int(max_items));
    json_object_object_add(result, "match_count", json_object_new_int((int)matches.count));
    json_object_object_add(result, "matches", arr);

    free_search_matches(&matches);
    return result;
}
