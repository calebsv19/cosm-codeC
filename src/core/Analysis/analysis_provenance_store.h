#ifndef ANALYSIS_PROVENANCE_STORE_H
#define ANALYSIS_PROVENANCE_STORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fisics_frontend.h"

#ifndef ANALYSIS_PROVENANCE_PATH_CAP
#define ANALYSIS_PROVENANCE_PATH_CAP 1024
#endif

typedef enum {
    ANALYSIS_AUTHORITY_CURRENT = 0,
    ANALYSIS_AUTHORITY_STALE,
    ANALYSIS_AUTHORITY_PARTIAL,
    ANALYSIS_AUTHORITY_DEGRADED,
    ANALYSIS_AUTHORITY_FATAL
} AnalysisAuthorityState;

typedef struct {
    char file_path[ANALYSIS_PROVENANCE_PATH_CAP];
    char contract_id[64];
    uint16_t contract_major;
    uint16_t contract_minor;
    uint16_t contract_patch;
    char producer_name[32];
    char producer_version[32];
    FisicsAnalysisMode mode;
    bool partial;
    bool fatal;
    uint64_t source_hash;
    uint64_t source_length;
    uint64_t advertised_capabilities;
    uint64_t effective_capabilities;
    bool capability_flags_present;
    bool source_match;
    bool loaded_from_cache;
    bool degraded;
    char degraded_reason[256];
    AnalysisAuthorityState state;
    uint64_t generation;
    uint64_t observed_at_unix_ms;
    uint64_t stamp;
} AnalysisProvenanceRecord;

typedef struct {
    size_t current_count;
    size_t stale_count;
    size_t partial_count;
    size_t degraded_count;
    size_t fatal_count;
    size_t total_count;
    AnalysisAuthorityState worst_state;
} AnalysisProvenanceSummary;

void analysis_provenance_store_clear(void);
void analysis_provenance_store_upsert(const char* file_path,
                                      const char* source,
                                      size_t source_length,
                                      const FisicsAnalysisContract* contract,
                                      uint64_t effective_capabilities,
                                      bool degraded,
                                      const char* degraded_reason);
void analysis_provenance_store_remove(const char* file_path);

size_t analysis_provenance_store_file_count(void);
bool analysis_provenance_store_copy_at(size_t index, AnalysisProvenanceRecord* out);
bool analysis_provenance_store_copy_for_file(const char* file_path,
                                             AnalysisProvenanceRecord* out);
// Produces one coherent, caller-owned copy of the records, summary, and stamp.
// Free a non-NULL records result with analysis_provenance_store_free_snapshot.
bool analysis_provenance_store_copy_snapshot(AnalysisProvenanceRecord** out_records,
                                             size_t* out_count,
                                             AnalysisProvenanceSummary* out_summary,
                                             uint64_t* out_stamp);
void analysis_provenance_store_free_snapshot(AnalysisProvenanceRecord* records);
void analysis_provenance_store_summary(AnalysisProvenanceSummary* out);
uint64_t analysis_provenance_store_combined_stamp(void);

const char* analysis_authority_state_name(AnalysisAuthorityState state);
const char* analysis_provenance_state_reason(const AnalysisProvenanceRecord* record);

void analysis_provenance_store_save(const char* workspace_root);
void analysis_provenance_store_load(const char* workspace_root);

#endif
