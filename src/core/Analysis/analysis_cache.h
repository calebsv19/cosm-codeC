#ifndef ANALYSIS_CACHE_H
#define ANALYSIS_CACHE_H

#include <stdbool.h>
#include <stdint.h>
#include <limits.h>

#include "core/Analysis/include_path_resolver.h"

#define ANALYSIS_CACHE_VERSION 1

typedef struct {
    uint32_t version;
    uint64_t build_args_hash;
    long makefile_mtime;
    char project_root[PATH_MAX];
} AnalysisCacheMeta;

// Metadata helpers
void analysis_cache_compute_meta(const char* workspace_root,
                                 const char* build_args,
                                 AnalysisCacheMeta* out);
bool analysis_cache_save_meta(const AnalysisCacheMeta* meta, const char* workspace_root);
bool analysis_cache_load_meta(AnalysisCacheMeta* out, const char* workspace_root);
bool analysis_cache_meta_matches(const AnalysisCacheMeta* meta,
                                 const char* workspace_root,
                                 const char* build_args);
bool analysis_cache_save_metadata(const char* workspace_root, const char* build_args);

// Persist individual artifacts into .ide_cache/. All return true on success.
bool analysis_cache_save_errors(const char* workspace_root);
bool analysis_cache_load_errors(const char* workspace_root, const char* build_args);

bool analysis_cache_save_symbols(const char* workspace_root);
bool analysis_cache_load_symbols(const char* workspace_root, const char* build_args);

bool analysis_cache_save_tokens(const char* workspace_root);
bool analysis_cache_load_tokens(const char* workspace_root, const char* build_args);

bool analysis_cache_save_library(const char* workspace_root);
bool analysis_cache_load_library(const char* workspace_root, const char* build_args);

bool analysis_cache_save_build_flags(const BuildFlagSet* flags, const char* workspace_root);
bool analysis_cache_load_build_flags(BuildFlagSet* flags,
                                     const char* workspace_root,
                                     const char* build_args);

#endif // ANALYSIS_CACHE_H
