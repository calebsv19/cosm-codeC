#include "core/Analysis/library_index.h"

#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <json-c/json.h>

static LibraryBucket g_buckets[LIB_BUCKET_COUNT];
static char g_project_root[PATH_MAX];
static pthread_mutex_t g_library_index_mutex = PTHREAD_MUTEX_INITIALIZER;

void library_index_lock(void) {
    pthread_mutex_lock(&g_library_index_mutex);
}

void library_index_unlock(void) {
    pthread_mutex_unlock(&g_library_index_mutex);
}

static void free_usage(LibraryUsage* u) {
    if (!u) return;
    free(u->source_path);
    u->source_path = NULL;
}

static void free_header(LibraryHeader* h) {
    if (!h) return;
    free(h->name);
    free(h->resolved_path);
    for (size_t i = 0; i < h->usage_count; ++i) {
        free_usage(&h->usages[i]);
    }
    free(h->usages);
    memset(h, 0, sizeof(*h));
}

static void free_buckets_locked(void) {
    for (size_t b = 0; b < LIB_BUCKET_COUNT; ++b) {
        LibraryBucket* bucket = &g_buckets[b];
        for (size_t h = 0; h < bucket->header_count; ++h) {
            free_header(&bucket->headers[h]);
        }
        free(bucket->headers);
        bucket->headers = NULL;
        bucket->header_count = 0;
        bucket->header_capacity = 0;
        bucket->kind = (LibraryBucketKind)b;
    }
}

static void library_index_reset_locked(void) {
    free_buckets_locked();
    memset(g_project_root, 0, sizeof(g_project_root));
}

void library_index_reset(void) {
    library_index_lock();
    library_index_reset_locked();
    library_index_unlock();
}

static void library_index_begin_locked(const char* project_root) {
    library_index_reset_locked();
    if (project_root && *project_root) {
        strncpy(g_project_root, project_root, sizeof(g_project_root) - 1);
        g_project_root[sizeof(g_project_root) - 1] = '\0';
    }
    for (size_t i = 0; i < LIB_BUCKET_COUNT; ++i) {
        g_buckets[i].kind = (LibraryBucketKind)i;
    }
}

void library_index_begin(const char* project_root) {
    library_index_lock();
    library_index_begin_locked(project_root);
    library_index_unlock();
}

static const char* skip_root_prefix(const char* path) {
    if (!path || !*path || !g_project_root[0]) return path;
    size_t root_len = strlen(g_project_root);
    if (strncmp(path, g_project_root, root_len) != 0) return path;
    // Accept path separator after root
    if (path[root_len] == '/' || path[root_len] == '\\') {
        return path + root_len + 1;
    }
    return path;
}

static LibraryBucket* bucket_for_kind(LibraryBucketKind kind) {
    if (kind < 0 || kind >= LIB_BUCKET_COUNT) return NULL;
    return &g_buckets[kind];
}

static LibraryHeader* find_header(LibraryBucket* bucket, const char* name) {
    if (!bucket || !name) return NULL;
    for (size_t i = 0; i < bucket->header_count; ++i) {
        if (bucket->headers[i].name && strcmp(bucket->headers[i].name, name) == 0) {
            return &bucket->headers[i];
        }
    }
    return NULL;
}

static LibraryHeader* create_header(LibraryBucket* bucket,
                                    const char* name,
                                    const char* resolved_path,
                                    LibraryIncludeKind kind,
                                    LibraryBucketKind origin) {
    if (!bucket || !name) return NULL;
    if (bucket->header_count >= bucket->header_capacity) {
        size_t new_cap = bucket->header_capacity ? bucket->header_capacity * 2 : 8;
        LibraryHeader* tmp = realloc(bucket->headers, new_cap * sizeof(LibraryHeader));
        if (!tmp) return NULL;
        bucket->headers = tmp;
        bucket->header_capacity = new_cap;
    }
    LibraryHeader* h = &bucket->headers[bucket->header_count++];
    memset(h, 0, sizeof(*h));
    h->name = strdup(name);
    h->resolved_path = resolved_path && *resolved_path ? strdup(resolved_path) : NULL;
    h->kind = kind;
    h->origin = origin;
    return h;
}

static bool add_usage_to_header(LibraryHeader* header,
                                const char* source_path,
                                int line,
                                int column) {
    if (!header || !source_path) return false;
    if (header->usage_count >= header->usage_capacity) {
        size_t new_cap = header->usage_capacity ? header->usage_capacity * 2 : 8;
        LibraryUsage* tmp = realloc(header->usages, new_cap * sizeof(LibraryUsage));
        if (!tmp) return false;
        header->usages = tmp;
        header->usage_capacity = new_cap;
    }
    LibraryUsage* u = &header->usages[header->usage_count++];
    u->source_path = strdup(source_path);
    u->line = line;
    u->column = column;
    return u->source_path != NULL;
}

static void library_index_add_include_locked(const char* source_path,
                                             const char* include_name,
                                             const char* resolved_path,
                                             LibraryIncludeKind kind,
                                             LibraryBucketKind origin,
                                             int line,
                                             int column) {
    if (!include_name || !source_path) return;
    LibraryBucket* bucket = bucket_for_kind(origin);
    if (!bucket) return;

    const char* rel = skip_root_prefix(source_path);
    if (!rel || !*rel) rel = source_path;

    LibraryHeader* header = find_header(bucket, include_name);
    if (!header) {
        header = create_header(bucket, include_name, resolved_path, kind, origin);
    } else if (!header->resolved_path && resolved_path && *resolved_path) {
        // Prefer a concrete resolved path if we encounter one later.
        header->resolved_path = strdup(resolved_path);
    }

    if (!header) return;
    add_usage_to_header(header, rel, line, column);
}

void library_index_add_include(const char* source_path,
                               const char* include_name,
                               const char* resolved_path,
                               LibraryIncludeKind kind,
                               LibraryBucketKind origin,
                               int line,
                               int column) {
    library_index_lock();
    library_index_add_include_locked(source_path, include_name, resolved_path, kind, origin, line, column);
    library_index_unlock();
}

void library_index_remove_source(const char* source_path) {
    if (!source_path || !*source_path) return;
    library_index_lock();
    const char* rel = skip_root_prefix(source_path);
    if (!rel || !*rel) rel = source_path;

    for (size_t b = 0; b < LIB_BUCKET_COUNT; ++b) {
        LibraryBucket* bucket = &g_buckets[b];
        size_t h = 0;
        while (h < bucket->header_count) {
            LibraryHeader* header = &bucket->headers[h];
            size_t u = 0;
            while (u < header->usage_count) {
                const char* src = header->usages[u].source_path ? header->usages[u].source_path : "";
                if (strcmp(src, rel) == 0) {
                    free_usage(&header->usages[u]);
                    for (size_t k = u + 1; k < header->usage_count; ++k) {
                        header->usages[k - 1] = header->usages[k];
                    }
                    header->usage_count--;
                    continue;
                }
                u++;
            }

            if (header->usage_count == 0) {
                free_header(header);
                for (size_t k = h + 1; k < bucket->header_count; ++k) {
                    bucket->headers[k - 1] = bucket->headers[k];
                }
                bucket->header_count--;
                continue;
            }
            h++;
        }
    }
    library_index_unlock();
}

static int cmp_headers(const void* a, const void* b) {
    const LibraryHeader* ha = (const LibraryHeader*)a;
    const LibraryHeader* hb = (const LibraryHeader*)b;
    if (!ha || !hb) return 0;
    if (!ha->name && !hb->name) return 0;
    if (!ha->name) return -1;
    if (!hb->name) return 1;
    return strcmp(ha->name, hb->name);
}

static int cmp_usages(const void* a, const void* b) {
    const LibraryUsage* ua = (const LibraryUsage*)a;
    const LibraryUsage* ub = (const LibraryUsage*)b;
    if (!ua || !ub) return 0;
    if (!ua->source_path && !ub->source_path) return 0;
    if (!ua->source_path) return -1;
    if (!ub->source_path) return 1;
    int path_cmp = strcmp(ua->source_path, ub->source_path);
    if (path_cmp != 0) return path_cmp;
    if (ua->line != ub->line) return (ua->line < ub->line) ? -1 : 1;
    return (ua->column < ub->column) ? -1 : (ua->column > ub->column);
}

static void library_index_finalize_locked(void) {
    for (size_t b = 0; b < LIB_BUCKET_COUNT; ++b) {
        LibraryBucket* bucket = &g_buckets[b];
        if (bucket->header_count > 1) {
            qsort(bucket->headers, bucket->header_count, sizeof(LibraryHeader), cmp_headers);
        }
        for (size_t h = 0; h < bucket->header_count; ++h) {
            LibraryHeader* header = &bucket->headers[h];
            if (header->usage_count > 1) {
                qsort(header->usages, header->usage_count, sizeof(LibraryUsage), cmp_usages);
            }
        }
    }
}

void library_index_finalize(void) {
    library_index_lock();
    library_index_finalize_locked();
    library_index_unlock();
}

// Query helpers

size_t library_index_bucket_count(void) {
    return LIB_BUCKET_COUNT;
}

const LibraryBucket* library_index_get_bucket(size_t index) {
    if (index >= LIB_BUCKET_COUNT) return NULL;
    return &g_buckets[index];
}

size_t library_index_header_count(const LibraryBucket* bucket) {
    return bucket ? bucket->header_count : 0;
}

const LibraryHeader* library_index_get_header(const LibraryBucket* bucket, size_t header_index) {
    if (!bucket || header_index >= bucket->header_count) return NULL;
    return &bucket->headers[header_index];
}

size_t library_index_usage_count(const LibraryHeader* header) {
    return header ? header->usage_count : 0;
}

const LibraryUsage* library_index_get_usage(const LibraryHeader* header, size_t usage_index) {
    if (!header || usage_index >= header->usage_count) return NULL;
    return &header->usages[usage_index];
}

// Persistence

static void ensure_cache_dir(const char* workspace_root) {
    if (!workspace_root || !*workspace_root) return;
    char dir[PATH_MAX];
    snprintf(dir, sizeof(dir), "%s/ide_files", workspace_root);
    struct stat st;
    if (stat(dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
        mkdir(dir, 0755);
    }
}

void library_index_save(const char* workspace_root) {
    if (!workspace_root || !*workspace_root) return;
    ensure_cache_dir(workspace_root);

    library_index_lock();
    json_object* root = json_object_new_object();
    json_object_object_add(root, "project_root", json_object_new_string(g_project_root));

    json_object* buckets = json_object_new_array();
    for (size_t b = 0; b < LIB_BUCKET_COUNT; ++b) {
        const LibraryBucket* bucket = &g_buckets[b];
        json_object* jb = json_object_new_object();
        json_object_object_add(jb, "kind", json_object_new_int(bucket->kind));
        json_object* headers = json_object_new_array();
        for (size_t h = 0; h < bucket->header_count; ++h) {
            const LibraryHeader* header = &bucket->headers[h];
            json_object* jh = json_object_new_object();
            json_object_object_add(jh, "name", json_object_new_string(header->name ? header->name : ""));
            json_object_object_add(jh, "resolved_path", json_object_new_string(header->resolved_path ? header->resolved_path : ""));
            json_object_object_add(jh, "kind", json_object_new_int(header->kind));
            json_object_object_add(jh, "origin", json_object_new_int(header->origin));
            json_object* usages = json_object_new_array();
            for (size_t u = 0; u < header->usage_count; ++u) {
                const LibraryUsage* usage = &header->usages[u];
                json_object* ju = json_object_new_object();
                json_object_object_add(ju, "source", json_object_new_string(usage->source_path ? usage->source_path : ""));
                json_object_object_add(ju, "line", json_object_new_int(usage->line));
                json_object_object_add(ju, "col", json_object_new_int(usage->column));
                json_object_array_add(usages, ju);
            }
            json_object_object_add(jh, "usages", usages);
            json_object_array_add(headers, jh);
        }
        json_object_object_add(jb, "headers", headers);
        json_object_array_add(buckets, jb);
    }
    json_object_object_add(root, "buckets", buckets);

    const char* serialized = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/ide_files/library_index.json", workspace_root);
    FILE* f = fopen(path, "w");
    if (f && serialized) {
        fputs(serialized, f);
        fclose(f);
    } else if (f) {
        fclose(f);
    }
    json_object_put(root);
    library_index_unlock();
}

void library_index_load(const char* workspace_root) {
    library_index_lock();
    library_index_reset_locked();
    if (!workspace_root || !*workspace_root) {
        library_index_unlock();
        return;
    }

    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/ide_files/library_index.json", workspace_root);
    FILE* f = fopen(path, "r");
    if (!f) {
        library_index_unlock();
        return;
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0 || len > (32 * 1024 * 1024)) {
        fclose(f);
        library_index_unlock();
        return;
    }
    char* buf = malloc((size_t)len + 1);
    if (!buf) {
        fclose(f);
        library_index_unlock();
        return;
    }
    fread(buf, 1, (size_t)len, f);
    buf[len] = '\0';
    fclose(f);

    json_object* root = json_tokener_parse(buf);
    free(buf);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        library_index_unlock();
        return;
    }

    json_object* jroot = NULL;
    if (json_object_object_get_ex(root, "project_root", &jroot)) {
        const char* pr = json_object_get_string(jroot);
        library_index_begin_locked(pr && *pr ? pr : workspace_root);
    } else {
        library_index_begin_locked(workspace_root);
    }

    json_object* jbuckets = NULL;
    if (json_object_object_get_ex(root, "buckets", &jbuckets) &&
        jbuckets && json_object_is_type(jbuckets, json_type_array)) {
        size_t bcount = json_object_array_length(jbuckets);
        for (size_t bi = 0; bi < bcount; ++bi) {
            json_object* jb = json_object_array_get_idx(jbuckets, bi);
            if (!jb || !json_object_is_type(jb, json_type_object)) continue;
            json_object* jheaders = NULL;
            json_object* jkind = NULL;
            LibraryBucketKind bucketKind = (LibraryBucketKind)bi;
            if (json_object_object_get_ex(jb, "kind", &jkind)) {
                bucketKind = (LibraryBucketKind)json_object_get_int(jkind);
            }
            if (!json_object_object_get_ex(jb, "headers", &jheaders) ||
                !jheaders || !json_object_is_type(jheaders, json_type_array)) {
                continue;
            }
            size_t hcount = json_object_array_length(jheaders);
            for (size_t hi = 0; hi < hcount; ++hi) {
                json_object* jh = json_object_array_get_idx(jheaders, hi);
                if (!jh || !json_object_is_type(jh, json_type_object)) continue;
                json_object* jname=NULL,* jres=NULL,* jkindH=NULL,* jorig=NULL,* jusages=NULL;
                json_object_object_get_ex(jh, "name", &jname);
                json_object_object_get_ex(jh, "resolved_path", &jres);
                json_object_object_get_ex(jh, "kind", &jkindH);
                json_object_object_get_ex(jh, "origin", &jorig);
                json_object_object_get_ex(jh, "usages", &jusages);
                const char* name = jname ? json_object_get_string(jname) : NULL;
                const char* resolved = jres ? json_object_get_string(jres) : NULL;
                LibraryIncludeKind kind = jkindH ? (LibraryIncludeKind)json_object_get_int(jkindH)
                                                 : LIB_INCLUDE_KIND_LOCAL;
                LibraryBucketKind origin = jorig ? (LibraryBucketKind)json_object_get_int(jorig)
                                                 : bucketKind;
                if (!name || !*name) continue;
                if (!jusages || !json_object_is_type(jusages, json_type_array)) continue;
                size_t ucount = json_object_array_length(jusages);
                for (size_t ui = 0; ui < ucount; ++ui) {
                    json_object* ju = json_object_array_get_idx(jusages, ui);
                    if (!ju || !json_object_is_type(ju, json_type_object)) continue;
                    json_object* jsrc=NULL,* jline=NULL,* jcol=NULL;
                    json_object_object_get_ex(ju, "source", &jsrc);
                    json_object_object_get_ex(ju, "line", &jline);
                    json_object_object_get_ex(ju, "col", &jcol);
                    const char* src = jsrc ? json_object_get_string(jsrc) : NULL;
                    int line = jline ? json_object_get_int(jline) : 0;
                    int col = jcol ? json_object_get_int(jcol) : 0;
                    library_index_add_include_locked(src ? src : "",
                                                     name,
                                                     (resolved && *resolved) ? resolved : NULL,
                                                     kind,
                                                     origin,
                                                     line,
                                                     col);
                }
            }
        }
    }
    library_index_finalize_locked();
    json_object_put(root);
    library_index_unlock();
}
