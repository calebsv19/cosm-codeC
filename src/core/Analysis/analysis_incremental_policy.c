#include "core/Analysis/analysis_incremental_policy.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static const AnalysisFileFingerprint* find_fingerprint(const AnalysisSnapshot* snapshot,
                                                       const char* path) {
    if (!snapshot || !path || !*path) return NULL;
    for (size_t i = 0; i < snapshot->file_count; ++i) {
        const AnalysisFileFingerprint* f = &snapshot->files[i];
        if (f->path && strcmp(f->path, path) == 0) {
            return f;
        }
    }
    return NULL;
}

static bool fingerprints_match(const AnalysisFileFingerprint* a,
                               const AnalysisFileFingerprint* b) {
    if (!a || !b) return false;
    if (a->content_hash != 0 && b->content_hash != 0) {
        return a->content_hash == b->content_hash;
    }
    return a->mtime == b->mtime && a->size == b->size;
}

static bool has_header_extension(const char* path) {
    if (!path) return false;
    size_t len = strlen(path);
    return len >= 2 && strcmp(path + len - 2, ".h") == 0;
}

static bool path_list_has_header(char* const* paths, size_t count) {
    if (!paths) return false;
    for (size_t i = 0; i < count; ++i) {
        if (has_header_extension(paths[i])) {
            return true;
        }
    }
    return false;
}

static bool const_path_list_has_header(const char* const* paths, size_t count) {
    if (!paths) return false;
    for (size_t i = 0; i < count; ++i) {
        if (has_header_extension(paths[i])) {
            return true;
        }
    }
    return false;
}

bool analysis_incremental_policy_should_analyze_hint(const AnalysisSnapshot* cached,
                                                     const AnalysisSnapshot* current,
                                                     const char* path) {
    if (!current || !path || !*path) return false;

    const AnalysisFileFingerprint* current_file = find_fingerprint(current, path);
    if (!current_file) {
        return false;
    }

    const AnalysisFileFingerprint* cached_file = find_fingerprint(cached, path);
    if (!cached_file) {
        return true;
    }

    return !fingerprints_match(cached_file, current_file);
}

bool analysis_incremental_policy_requires_full_for_missing_include_graph(
    size_t include_graph_entry_count,
    char* const* dirty_paths,
    size_t dirty_count,
    char* const* removed_paths,
    size_t removed_count,
    const char* const* file_hints,
    size_t file_hint_count) {
    if (include_graph_entry_count > 0) {
        return false;
    }
    if (path_list_has_header(dirty_paths, dirty_count)) {
        return true;
    }
    if (path_list_has_header(removed_paths, removed_count)) {
        return true;
    }
    return const_path_list_has_header(file_hints, file_hint_count);
}
