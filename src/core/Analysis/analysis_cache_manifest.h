#ifndef ANALYSIS_CACHE_MANIFEST_H
#define ANALYSIS_CACHE_MANIFEST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/Analysis/analysis_token_store.h"
#include "core/Analysis/analysis_startup_audit.h"

#define ANALYSIS_CACHE_MANIFEST_VERSION 1
#define ANALYSIS_CACHE_MANIFEST_MAX_ARTIFACTS 16
#define ANALYSIS_CACHE_TOKEN_LOAD_LIMIT_BYTES ((long long)ANALYSIS_TOKEN_STORE_PERSIST_LIMIT_BYTES)

typedef struct {
    char name[64];
    char relative_path[160];
    bool present;
    long long size_bytes;
    char trust_state[24];
    char reason[96];
} AnalysisCacheManifestArtifact;

typedef struct {
    uint32_t version;
    char project_root[1024];
    char phase[32];
    long long generated_unix_time;
    bool cache_meta_present;
    bool cache_meta_trusted;
    char cache_meta_state[24];
    char cache_meta_reason[96];
    bool has_startup_audit;
    AnalysisStartupAudit startup_audit;
    AnalysisCacheManifestArtifact artifacts[ANALYSIS_CACHE_MANIFEST_MAX_ARTIFACTS];
    size_t artifact_count;
} AnalysisCacheManifestReport;

bool analysis_cache_manifest_evaluate(const char* workspace_root,
                                      const char* build_args,
                                      const char* phase,
                                      AnalysisCacheManifestReport* out_report);
bool analysis_cache_manifest_save(const AnalysisCacheManifestReport* report);
bool analysis_cache_manifest_write_report(const char* workspace_root,
                                          const char* build_args,
                                          const char* phase);
const AnalysisCacheManifestArtifact*
analysis_cache_manifest_find_artifact(const AnalysisCacheManifestReport* report,
                                      const char* name);

#endif // ANALYSIS_CACHE_MANIFEST_H
