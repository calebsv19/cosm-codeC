#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "core/Analysis/analysis_provenance_store.h"
#include "core/Analysis/analysis_token_store.h"

static int failures = 0;

static void* token_writer(void* unused) {
    (void)unused;
    for (int i = 0; i < 2000; ++i) {
        FisicsTokenSpan span = {.line = i + 1, .column = 2, .length = 3,
                                .kind = FISICS_TOK_IDENTIFIER};
        analysis_token_store_upsert("/tmp/s2/concurrent.c", &span, 1);
    }
    return NULL;
}

int main(void) {
    const char* source = "int stable;\n";
    FisicsAnalysisContract contract = {0};
    snprintf(contract.contract_id, sizeof(contract.contract_id),
             "fisiCs.analysis.contract");
    contract.contract_major = 1;
    contract.contract_minor = 7;
    contract.source_length = strlen(source);
    contract.source_hash = 0x8f0fb8bad1ee7883ULL;
    contract.capabilities = FISICS_CONTRACT_CAP_DIAGNOSTICS;
    analysis_provenance_store_upsert("/tmp/s2/stable.c", source, strlen(source),
                                     &contract, contract.capabilities, false, NULL);

    AnalysisProvenanceRecord* snapshot = NULL;
    size_t snapshot_count = 0;
    AnalysisProvenanceSummary summary = {0};
    uint64_t snapshot_stamp = 0;
    if (!analysis_provenance_store_copy_snapshot(&snapshot, &snapshot_count,
                                                  &summary, &snapshot_stamp) ||
        snapshot_count != 1) {
        fprintf(stderr, "FAIL: provenance snapshot capture\n");
        return 1;
    }
    analysis_provenance_store_clear();
    if (strcmp(snapshot[0].file_path, "/tmp/s2/stable.c") != 0 ||
        summary.total_count != 1) {
        fprintf(stderr, "FAIL: provenance snapshot changed after store clear\n");
        failures++;
    }
    analysis_provenance_store_free_snapshot(snapshot);

    pthread_t writer;
    pthread_create(&writer, NULL, token_writer, NULL);
    for (int i = 0; i < 2000; ++i) {
        analysis_token_store_lock();
        size_t count = analysis_token_store_file_count();
        if (count > 0) {
            const AnalysisFileTokens* file = analysis_token_store_file_at(0);
            if (!file || !file->path || file->count != 1 || !file->spans) failures++;
        }
        analysis_token_store_unlock();
    }
    pthread_join(writer, NULL);
    analysis_token_store_clear();

    if (failures) return 1;
    puts("analysis copy-out concurrency test passed");
    return 0;
}
