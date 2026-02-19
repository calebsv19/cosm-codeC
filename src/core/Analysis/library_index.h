#ifndef LIBRARY_INDEX_H
#define LIBRARY_INDEX_H

#include <stddef.h>
#include <stdbool.h>

#include "core/Analysis/include_path_resolver.h"

// Buckets correspond to where an include was resolved (or not).
typedef enum {
    LIB_BUCKET_PROJECT = 0,
    LIB_BUCKET_SYSTEM,
    LIB_BUCKET_EXTERNAL,
    LIB_BUCKET_UNRESOLVED,
    LIB_BUCKET_COUNT
} LibraryBucketKind;

// Include spelling type.
typedef enum {
    LIB_INCLUDE_KIND_LOCAL = 0, // #include "..."
    LIB_INCLUDE_KIND_SYSTEM      // #include <...>
} LibraryIncludeKind;

typedef struct {
    char* source_path;   // Relative to project root when possible
    int line;
    int column;
} LibraryUsage;

typedef struct {
    char* name;               // Header name as written (e.g., stdio.h, SDL2/SDL.h)
    char* resolved_path;      // Absolute/canonical path if resolved; NULL otherwise
    LibraryIncludeKind kind;  // Local vs system form
    LibraryBucketKind origin; // Bucket classification
    LibraryUsage* usages;
    size_t usage_count;
    size_t usage_capacity;
} LibraryHeader;

typedef struct {
    LibraryBucketKind kind;
    LibraryHeader* headers;
    size_t header_count;
    size_t header_capacity;
} LibraryBucket;

// Lifecycle
void library_index_reset(void);                 // Clear all buckets/headers/usages
void library_index_begin(const char* project_root); // Set project root used for relative paths
void library_index_finalize(void);              // Sort headers/usages after population
// Synchronization helpers (call to guard reads during background analysis).
void library_index_lock(void);
void library_index_unlock(void);
// Persistence (ide_files/library_index.json). Safe to call even if no data exists.
void library_index_save(const char* workspace_root);
void library_index_load(const char* workspace_root);

// Mutation
void library_index_add_include(const char* source_path,
                               const char* include_name,
                               const char* resolved_path,
                               LibraryIncludeKind kind,
                               LibraryBucketKind origin,
                               int line,
                               int column);

typedef struct {
    size_t files_seen;
    size_t files_analyzed;
    size_t includes_added;
    size_t read_failures;
    size_t analysis_failures;
} LibraryIndexBuildStats;

// Returns the stats from the most recent build (zeroed if none have run).
void library_index_get_last_build_stats(LibraryIndexBuildStats* outStats);

// Queries (read-only views; pointers remain valid until next reset/begin)
// Call library_index_lock/library_index_unlock to guard multi-step reads.
size_t library_index_bucket_count(void);
const LibraryBucket* library_index_get_bucket(size_t index);

size_t library_index_header_count(const LibraryBucket* bucket);
const LibraryHeader* library_index_get_header(const LibraryBucket* bucket, size_t header_index);

size_t library_index_usage_count(const LibraryHeader* header);
const LibraryUsage* library_index_get_usage(const LibraryHeader* header, size_t usage_index);

// Build helpers
void library_index_build_workspace(const char* project_root);
void library_index_build_workspace_with_flags(const char* project_root, const BuildFlagSet* flags);

#endif // LIBRARY_INDEX_H
