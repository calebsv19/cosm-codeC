#include "core/Analysis/library_index.h"

#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static LibraryBucket g_buckets[LIB_BUCKET_COUNT];
static char g_project_root[PATH_MAX];

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

static void free_buckets(void) {
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

void library_index_reset(void) {
    free_buckets();
    memset(g_project_root, 0, sizeof(g_project_root));
}

void library_index_begin(const char* project_root) {
    library_index_reset();
    if (project_root && *project_root) {
        strncpy(g_project_root, project_root, sizeof(g_project_root) - 1);
        g_project_root[sizeof(g_project_root) - 1] = '\0';
    }
    for (size_t i = 0; i < LIB_BUCKET_COUNT; ++i) {
        g_buckets[i].kind = (LibraryBucketKind)i;
    }
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

void library_index_add_include(const char* source_path,
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

void library_index_finalize(void) {
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
