#ifndef ANALYSIS_SNAPSHOT_H
#define ANALYSIS_SNAPSHOT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ANALYSIS_SNAPSHOT_VERSION 1

typedef struct {
    char* path;
    long mtime;
    long long size;
    uint64_t content_hash;
} AnalysisFileFingerprint;

typedef struct {
    uint32_t version;
    long generated_at_unix;
    AnalysisFileFingerprint* files;
    size_t file_count;
    size_t file_capacity;
} AnalysisSnapshot;

void analysis_snapshot_init(AnalysisSnapshot* snapshot);
void analysis_snapshot_clear(AnalysisSnapshot* snapshot);

bool analysis_snapshot_load(const char* workspace_root, AnalysisSnapshot* out_snapshot);
bool analysis_snapshot_save(const char* workspace_root, const AnalysisSnapshot* snapshot);

bool analysis_snapshot_scan_workspace(const char* workspace_root, AnalysisSnapshot* out_snapshot);
bool analysis_snapshot_refresh_and_save(const char* workspace_root);

bool analysis_snapshot_compute_dirty_sets(const AnalysisSnapshot* cached,
                                          const AnalysisSnapshot* current,
                                          char*** out_dirty_paths,
                                          size_t* out_dirty_count,
                                          char*** out_removed_paths,
                                          size_t* out_removed_count);
void analysis_snapshot_free_path_list(char** paths, size_t count);

#endif // ANALYSIS_SNAPSHOT_H
