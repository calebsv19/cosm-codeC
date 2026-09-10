#include "core/Analysis/analysis_provenance_store.h"

#include "test_fixture_utils.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;

static void check(bool condition, const char* message) {
    if (condition) return;
    fprintf(stderr, "analysis_provenance_store_test: %s\n", message);
    failures++;
}

static uint64_t source_hash(const char* text) {
    const uint64_t prime = 1099511628211ULL;
    uint64_t hash = 1469598103934665603ULL;
    size_t length = strlen(text);
    for (size_t i = 0; i < length; ++i) {
        hash ^= (unsigned char)text[i];
        hash *= prime;
    }
    return hash;
}

static FisicsAnalysisContract contract_for(const char* source) {
    FisicsAnalysisContract contract = {0};
    snprintf(contract.contract_id, sizeof(contract.contract_id), "%s", "fisiCs.analysis.contract");
    contract.contract_major = 1;
    contract.contract_minor = 7;
    contract.contract_patch = 0;
    snprintf(contract.producer_name, sizeof(contract.producer_name), "%s", "fisiCs");
    snprintf(contract.producer_version, sizeof(contract.producer_version), "%s", "test");
    contract.mode = FISICS_ANALYSIS_MODE_LENIENT;
    contract.source_length = strlen(source);
    contract.source_hash = source_hash(source);
    contract.capabilities = FISICS_CONTRACT_CAP_DIAGNOSTICS |
                            FISICS_CONTRACT_CAP_INCLUDES |
                            FISICS_CONTRACT_CAP_SYMBOLS |
                            FISICS_CONTRACT_CAP_TOKENS;
    return contract;
}

static void expect_state(const char* file, AnalysisAuthorityState expected) {
    AnalysisProvenanceRecord record = {0};
    check(analysis_provenance_store_copy_for_file(file, &record), "missing expected file record");
    check(record.state == expected, "unexpected authority state");
}

int main(void) {
    char workspace[1024];
    check(ide_test_prepare_workspace(workspace,
                                     sizeof(workspace),
                                     "ide_analysis_provenance_store_test"),
          "failed to prepare workspace");
    const char* source = "int main(void) { return 0; }\n";
    FisicsAnalysisContract contract = contract_for(source);
    uint64_t effective = contract.capabilities;

    analysis_provenance_store_clear();
    analysis_provenance_store_upsert("/tmp/provenance/current.c", source, strlen(source),
                                     &contract, effective, false, NULL);

    FisicsAnalysisContract stale = contract;
    stale.source_hash++;
    analysis_provenance_store_upsert("/tmp/provenance/stale.c", source, strlen(source),
                                     &stale, effective, false, NULL);

    FisicsAnalysisContract partial = contract;
    partial.partial = true;
    analysis_provenance_store_upsert("/tmp/provenance/partial.c", source, strlen(source),
                                     &partial, effective, false, NULL);

    analysis_provenance_store_upsert("/tmp/provenance/degraded.c", source, strlen(source),
                                     &contract, 0, true, "unsupported contract major 2");

    FisicsAnalysisContract fatal = contract;
    fatal.fatal = true;
    analysis_provenance_store_upsert("/tmp/provenance/fatal.c", source, strlen(source),
                                     &fatal, effective, false, NULL);

    expect_state("/tmp/provenance/current.c", ANALYSIS_AUTHORITY_CURRENT);
    expect_state("/tmp/provenance/stale.c", ANALYSIS_AUTHORITY_STALE);
    expect_state("/tmp/provenance/partial.c", ANALYSIS_AUTHORITY_PARTIAL);
    expect_state("/tmp/provenance/degraded.c", ANALYSIS_AUTHORITY_DEGRADED);
    expect_state("/tmp/provenance/fatal.c", ANALYSIS_AUTHORITY_FATAL);

    AnalysisProvenanceSummary summary = {0};
    analysis_provenance_store_summary(&summary);
    check(summary.total_count == 5, "expected five records");
    check(summary.current_count == 1, "expected one current record");
    check(summary.stale_count == 1, "expected one stale record");
    check(summary.partial_count == 1, "expected one partial record");
    check(summary.degraded_count == 1, "expected one degraded record");
    check(summary.fatal_count == 1, "expected one fatal record");
    check(summary.worst_state == ANALYSIS_AUTHORITY_FATAL, "expected fatal worst state");

    AnalysisProvenanceRecord before = {0};
    check(analysis_provenance_store_copy_for_file("/tmp/provenance/current.c", &before),
          "missing current record before save");
    analysis_provenance_store_save(workspace);
    analysis_provenance_store_clear();
    analysis_provenance_store_load(workspace);

    AnalysisProvenanceRecord cached = {0};
    check(analysis_provenance_store_copy_for_file("/tmp/provenance/current.c", &cached),
          "missing cached current record");
    check(cached.loaded_from_cache, "loaded record must be identified as cached");
    check(!cached.source_match, "cached record must await source verification");
    check(cached.state == ANALYSIS_AUTHORITY_STALE, "cached current record must become stale");
    check(strcmp(analysis_provenance_state_reason(&cached),
                 "cached result awaiting source verification") == 0,
          "cached stale reason mismatch");

    analysis_provenance_store_upsert("/tmp/provenance/current.c", source, strlen(source),
                                     &contract, effective, false, NULL);
    AnalysisProvenanceRecord refreshed = {0};
    check(analysis_provenance_store_copy_for_file("/tmp/provenance/current.c", &refreshed),
          "missing refreshed record");
    check(refreshed.state == ANALYSIS_AUTHORITY_CURRENT, "refreshed record must be current");
    check(refreshed.generation > before.generation, "generation must advance after reload refresh");

    analysis_provenance_store_remove("/tmp/provenance/current.c");
    check(!analysis_provenance_store_copy_for_file("/tmp/provenance/current.c", &refreshed),
          "removed record remained visible");
    analysis_provenance_store_clear();

    if (failures != 0) return 1;
    puts("analysis_provenance_store_test: ok");
    return 0;
}
