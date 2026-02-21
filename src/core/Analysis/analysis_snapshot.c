#include "core/Analysis/analysis_snapshot.h"

#include <dirent.h>
#include <json-c/json.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>

static void ensure_ide_dir(const char* workspace_root) {
    if (!workspace_root || !*workspace_root) return;
    char dir[PATH_MAX];
    snprintf(dir, sizeof(dir), "%s/ide_files", workspace_root);
    struct stat st;
    if (stat(dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
        mkdir(dir, 0755);
    }
}

static bool has_ext(const char* name, const char* ext) {
    size_t ln = strlen(name);
    size_t le = strlen(ext);
    if (ln < le) return false;
    return strcasecmp(name + ln - le, ext) == 0;
}

static bool is_source_file(const char* name) {
    return has_ext(name, ".c") || has_ext(name, ".h");
}

static bool should_skip_dir(const char* name) {
    return strcmp(name, ".") == 0 ||
           strcmp(name, "..") == 0 ||
           strcmp(name, "build") == 0 ||
           strcmp(name, "ide_files") == 0 ||
           strcmp(name, ".git") == 0 ||
           strcmp(name, ".DS_Store") == 0;
}

static bool is_allowed_root_dir(const char* name) {
    if (!name) return false;
    return strcmp(name, "src") == 0 || strcmp(name, "include") == 0;
}

static bool snapshot_reserve(AnalysisSnapshot* snapshot, size_t count) {
    if (!snapshot) return false;
    if (snapshot->file_count + count <= snapshot->file_capacity) return true;
    size_t new_capacity = snapshot->file_capacity ? snapshot->file_capacity * 2 : 64;
    while (new_capacity < snapshot->file_count + count) {
        new_capacity *= 2;
    }
    AnalysisFileFingerprint* grown =
        (AnalysisFileFingerprint*)realloc(snapshot->files, new_capacity * sizeof(AnalysisFileFingerprint));
    if (!grown) return false;
    snapshot->files = grown;
    snapshot->file_capacity = new_capacity;
    return true;
}

static bool snapshot_add_file(AnalysisSnapshot* snapshot,
                              const char* path,
                              long mtime,
                              long long size,
                              uint64_t hash) {
    if (!snapshot || !path || !*path) return false;
    if (!snapshot_reserve(snapshot, 1)) return false;
    AnalysisFileFingerprint* out = &snapshot->files[snapshot->file_count++];
    memset(out, 0, sizeof(*out));
    out->path = strdup(path);
    if (!out->path) {
        snapshot->file_count--;
        return false;
    }
    out->mtime = mtime;
    out->size = size;
    out->content_hash = hash;
    return true;
}

static int cmp_file_path(const void* a, const void* b) {
    const AnalysisFileFingerprint* fa = (const AnalysisFileFingerprint*)a;
    const AnalysisFileFingerprint* fb = (const AnalysisFileFingerprint*)b;
    const char* pa = (fa && fa->path) ? fa->path : "";
    const char* pb = (fb && fb->path) ? fb->path : "";
    return strcmp(pa, pb);
}

static void snapshot_sort(AnalysisSnapshot* snapshot) {
    if (!snapshot || snapshot->file_count < 2) return;
    qsort(snapshot->files, snapshot->file_count, sizeof(AnalysisFileFingerprint), cmp_file_path);
}

static bool append_path(char*** paths, size_t* count, size_t* capacity, const char* path) {
    if (!paths || !count || !capacity || !path || !*path) return false;
    if (*count >= *capacity) {
        size_t new_capacity = *capacity ? (*capacity * 2) : 32;
        char** grown = (char**)realloc(*paths, new_capacity * sizeof(char*));
        if (!grown) return false;
        *paths = grown;
        *capacity = new_capacity;
    }
    (*paths)[*count] = strdup(path);
    if (!(*paths)[*count]) return false;
    (*count)++;
    return true;
}

void analysis_snapshot_init(AnalysisSnapshot* snapshot) {
    if (!snapshot) return;
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->version = ANALYSIS_SNAPSHOT_VERSION;
    snapshot->generated_at_unix = time(NULL);
}

void analysis_snapshot_clear(AnalysisSnapshot* snapshot) {
    if (!snapshot) return;
    for (size_t i = 0; i < snapshot->file_count; ++i) {
        free(snapshot->files[i].path);
        snapshot->files[i].path = NULL;
    }
    free(snapshot->files);
    analysis_snapshot_init(snapshot);
}

typedef struct {
    char* path;
    int depth;
} DirQueueEntry;

bool analysis_snapshot_scan_workspace(const char* workspace_root, AnalysisSnapshot* out_snapshot) {
    if (!workspace_root || !*workspace_root || !out_snapshot) return false;
    analysis_snapshot_clear(out_snapshot);

    size_t count = 0;
    size_t capacity = 32;
    DirQueueEntry* stack = (DirQueueEntry*)malloc(capacity * sizeof(DirQueueEntry));
    if (!stack) return false;
    stack[count++] = (DirQueueEntry){strdup(workspace_root), 0};

    while (count > 0) {
        DirQueueEntry cur = stack[--count];
        if (!cur.path) continue;

        DIR* dir = opendir(cur.path);
        if (!dir) {
            free(cur.path);
            continue;
        }

        struct dirent* ent = NULL;
        char child[PATH_MAX];
        while ((ent = readdir(dir)) != NULL) {
            if (should_skip_dir(ent->d_name)) continue;
            if (cur.depth == 0 && !is_allowed_root_dir(ent->d_name)) continue;

            snprintf(child, sizeof(child), "%s/%s", cur.path, ent->d_name);
            struct stat st;
            if (stat(child, &st) != 0) continue;

            if (S_ISDIR(st.st_mode)) {
                if (count >= capacity) {
                    size_t new_cap = capacity * 2;
                    DirQueueEntry* grown =
                        (DirQueueEntry*)realloc(stack, new_cap * sizeof(DirQueueEntry));
                    if (!grown) continue;
                    stack = grown;
                    capacity = new_cap;
                }
                stack[count++] = (DirQueueEntry){strdup(child), cur.depth + 1};
            } else if (S_ISREG(st.st_mode) && is_source_file(ent->d_name)) {
                if (!snapshot_add_file(out_snapshot, child, (long)st.st_mtime, (long long)st.st_size, 0)) {
                    closedir(dir);
                    free(cur.path);
                    free(stack);
                    analysis_snapshot_clear(out_snapshot);
                    return false;
                }
            }
        }
        closedir(dir);
        free(cur.path);
    }

    free(stack);
    out_snapshot->generated_at_unix = time(NULL);
    snapshot_sort(out_snapshot);
    return true;
}

bool analysis_snapshot_save(const char* workspace_root, const AnalysisSnapshot* snapshot) {
    if (!workspace_root || !*workspace_root || !snapshot) return false;
    ensure_ide_dir(workspace_root);

    json_object* root = json_object_new_object();
    json_object_object_add(root, "version", json_object_new_int((int)snapshot->version));
    json_object_object_add(root, "generated_at_unix",
                           json_object_new_int64((long long)snapshot->generated_at_unix));

    json_object* files = json_object_new_array();
    for (size_t i = 0; i < snapshot->file_count; ++i) {
        const AnalysisFileFingerprint* f = &snapshot->files[i];
        json_object* obj = json_object_new_object();
        json_object_object_add(obj, "path", json_object_new_string(f->path ? f->path : ""));
        json_object_object_add(obj, "mtime", json_object_new_int64((long long)f->mtime));
        json_object_object_add(obj, "size", json_object_new_int64((long long)f->size));
        json_object_object_add(obj, "hash", json_object_new_int64((long long)f->content_hash));
        json_object_array_add(files, obj);
    }
    json_object_object_add(root, "files", files);

    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/ide_files/index.json", workspace_root);
    FILE* fp = fopen(path, "w");
    if (!fp) {
        json_object_put(root);
        return false;
    }

    const char* serialized = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
    if (!serialized) {
        fclose(fp);
        json_object_put(root);
        return false;
    }

    fputs(serialized, fp);
    fclose(fp);
    json_object_put(root);
    return true;
}

bool analysis_snapshot_load(const char* workspace_root, AnalysisSnapshot* out_snapshot) {
    if (!workspace_root || !*workspace_root || !out_snapshot) return false;
    analysis_snapshot_clear(out_snapshot);

    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/ide_files/index.json", workspace_root);
    FILE* fp = fopen(path, "r");
    if (!fp) return false;

    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (len <= 0 || len > (32 * 1024 * 1024)) {
        fclose(fp);
        return false;
    }

    char* buf = (char*)malloc((size_t)len + 1);
    if (!buf) {
        fclose(fp);
        return false;
    }
    fread(buf, 1, (size_t)len, fp);
    buf[len] = '\0';
    fclose(fp);

    json_object* root = json_tokener_parse(buf);
    free(buf);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        return false;
    }

    json_object* jversion = NULL;
    json_object* jgenerated = NULL;
    json_object* jfiles = NULL;
    json_object_object_get_ex(root, "version", &jversion);
    json_object_object_get_ex(root, "generated_at_unix", &jgenerated);
    json_object_object_get_ex(root, "files", &jfiles);

    out_snapshot->version = jversion ? (uint32_t)json_object_get_int(jversion)
                                     : ANALYSIS_SNAPSHOT_VERSION;
    out_snapshot->generated_at_unix = jgenerated ? (long)json_object_get_int64(jgenerated) : 0;
    if (!jfiles || !json_object_is_type(jfiles, json_type_array)) {
        json_object_put(root);
        return true;
    }

    size_t count = json_object_array_length(jfiles);
    for (size_t i = 0; i < count; ++i) {
        json_object* obj = json_object_array_get_idx(jfiles, i);
        if (!obj || !json_object_is_type(obj, json_type_object)) continue;
        json_object* jpath = NULL;
        json_object* jmtime = NULL;
        json_object* jsize = NULL;
        json_object* jhash = NULL;
        json_object_object_get_ex(obj, "path", &jpath);
        json_object_object_get_ex(obj, "mtime", &jmtime);
        json_object_object_get_ex(obj, "size", &jsize);
        json_object_object_get_ex(obj, "hash", &jhash);
        const char* file_path = jpath ? json_object_get_string(jpath) : NULL;
        if (!file_path || !*file_path) continue;
        long mtime = jmtime ? (long)json_object_get_int64(jmtime) : 0;
        long long size = jsize ? (long long)json_object_get_int64(jsize) : 0;
        uint64_t hash = jhash ? (uint64_t)json_object_get_int64(jhash) : 0;
        if (!snapshot_add_file(out_snapshot, file_path, mtime, size, hash)) {
            json_object_put(root);
            analysis_snapshot_clear(out_snapshot);
            return false;
        }
    }

    json_object_put(root);
    snapshot_sort(out_snapshot);
    return true;
}

bool analysis_snapshot_refresh_and_save(const char* workspace_root) {
    if (!workspace_root || !*workspace_root) return false;
    AnalysisSnapshot snapshot = {0};
    analysis_snapshot_init(&snapshot);
    bool ok = analysis_snapshot_scan_workspace(workspace_root, &snapshot) &&
              analysis_snapshot_save(workspace_root, &snapshot);
    analysis_snapshot_clear(&snapshot);
    return ok;
}

bool analysis_snapshot_compute_dirty_sets(const AnalysisSnapshot* cached,
                                          const AnalysisSnapshot* current,
                                          char*** out_dirty_paths,
                                          size_t* out_dirty_count,
                                          char*** out_removed_paths,
                                          size_t* out_removed_count) {
    if (!cached || !current || !out_dirty_paths || !out_dirty_count ||
        !out_removed_paths || !out_removed_count) {
        return false;
    }

    *out_dirty_paths = NULL;
    *out_dirty_count = 0;
    *out_removed_paths = NULL;
    *out_removed_count = 0;
    size_t dirty_capacity = 0;
    size_t removed_capacity = 0;

    size_t i = 0;
    size_t j = 0;
    while (i < cached->file_count && j < current->file_count) {
        const AnalysisFileFingerprint* a = &cached->files[i];
        const AnalysisFileFingerprint* b = &current->files[j];
        const char* ap = a->path ? a->path : "";
        const char* bp = b->path ? b->path : "";
        int cmp = strcmp(ap, bp);
        if (cmp == 0) {
            if (a->mtime != b->mtime || a->size != b->size) {
                if (!append_path(out_dirty_paths, out_dirty_count, &dirty_capacity, bp)) return false;
            }
            i++;
            j++;
            continue;
        }
        if (cmp < 0) {
            if (!append_path(out_removed_paths, out_removed_count, &removed_capacity, ap)) return false;
            i++;
        } else {
            if (!append_path(out_dirty_paths, out_dirty_count, &dirty_capacity, bp)) return false;
            j++;
        }
    }

    while (i < cached->file_count) {
        const char* ap = cached->files[i].path ? cached->files[i].path : "";
        if (!append_path(out_removed_paths, out_removed_count, &removed_capacity, ap)) return false;
        i++;
    }
    while (j < current->file_count) {
        const char* bp = current->files[j].path ? current->files[j].path : "";
        if (!append_path(out_dirty_paths, out_dirty_count, &dirty_capacity, bp)) return false;
        j++;
    }

    return true;
}

void analysis_snapshot_free_path_list(char** paths, size_t count) {
    if (!paths) return;
    for (size_t i = 0; i < count; ++i) {
        free(paths[i]);
    }
    free(paths);
}
