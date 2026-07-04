#include "core/Analysis/analysis_cache_manifest.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "core/Analysis/analysis_cache.h"
#include "core/Analysis/include_path_resolver.h"
#include "test_fixture_utils.h"

void analysis_store_save(const char* workspaceRoot) { (void)workspaceRoot; }
void analysis_store_load(const char* workspaceRoot) { (void)workspaceRoot; }
void analysis_symbols_store_save(const char* workspaceRoot) { (void)workspaceRoot; }
void analysis_symbols_store_load(const char* workspaceRoot) { (void)workspaceRoot; }
void analysis_units_store_save(const char* workspaceRoot) { (void)workspaceRoot; }
void analysis_units_store_load(const char* workspaceRoot) { (void)workspaceRoot; }
void analysis_token_store_save(const char* workspaceRoot) { (void)workspaceRoot; }
void analysis_token_store_load(const char* workspaceRoot) { (void)workspaceRoot; }
void library_index_save(const char* workspaceRoot) { (void)workspaceRoot; }
void library_index_load(const char* workspaceRoot) { (void)workspaceRoot; }
void save_build_flags(const BuildFlagSet* flags, const char* workspaceRoot) {
    (void)flags;
    (void)workspaceRoot;
}
void load_build_flags(BuildFlagSet* flags, const char* workspaceRoot) {
    (void)flags;
    (void)workspaceRoot;
}

static int expect(int condition, const char* message) {
    if (!condition) {
        fprintf(stderr, "%s\n", message);
        return 1;
    }
    return 0;
}

static int artifact_has_state(const AnalysisCacheManifestReport* report,
                              const char* name,
                              const char* state,
                              const char* reason) {
    const AnalysisCacheManifestArtifact* artifact =
        analysis_cache_manifest_find_artifact(report, name);
    if (expect(artifact != NULL, "expected artifact entry")) return 1;
    if (expect(strcmp(artifact->trust_state, state) == 0, "unexpected artifact trust state")) return 1;
    if (expect(strcmp(artifact->reason, reason) == 0, "unexpected artifact reason")) return 1;
    return 0;
}

int main(void) {
    char root[256];
    if (expect(ide_test_prepare_workspace(root, sizeof(root), "ide_cache_manifest_test"),
               "expected workspace setup")) return 1;

    AnalysisCacheManifestReport report;
    if (expect(analysis_cache_manifest_evaluate(root, NULL, "startup", &report),
               "expected missing-meta evaluation to succeed")) return 1;
    if (expect(!report.cache_meta_present, "expected cache meta to be missing")) return 1;
    if (expect(!report.cache_meta_trusted, "expected missing cache meta to be untrusted")) return 1;
    if (expect(strcmp(report.cache_meta_reason, "cache_meta_missing") == 0,
               "expected missing cache meta reason")) return 1;
    if (artifact_has_state(&report, "diagnostics", "missing", "artifact_missing")) return 1;

    if (expect(analysis_cache_save_metadata(root, NULL), "expected metadata save")) return 1;

    char path[384];
    snprintf(path, sizeof(path), "%s/ide_files/analysis_diagnostics.json", root);
    if (expect(ide_test_write_text_file(path, "[]"), "expected diagnostics artifact write")) return 1;
    snprintf(path, sizeof(path), "%s/ide_files/index.json", root);
    if (expect(ide_test_write_text_file(path, "{}"), "expected source index artifact write")) return 1;

    if (expect(analysis_cache_manifest_evaluate(root, NULL, "analysis_persisted", &report),
               "expected trusted evaluation to succeed")) return 1;
    if (expect(report.cache_meta_present, "expected cache meta present")) return 1;
    if (expect(report.cache_meta_trusted, "expected cache meta trusted")) return 1;
    if (expect(strcmp(report.cache_meta_reason, "accepted") == 0, "expected accepted meta reason")) return 1;
    if (artifact_has_state(&report, "diagnostics", "accepted", "accepted")) return 1;
    if (artifact_has_state(&report, "source_index", "accepted", "accepted")) return 1;

    snprintf(path, sizeof(path), "%s/ide_files/analysis_tokens.json", root);
    if (expect(ide_test_write_sparse_file(path, ANALYSIS_CACHE_TOKEN_LOAD_LIMIT_BYTES + 1),
               "expected oversized token artifact write")) return 1;
    if (expect(analysis_cache_manifest_evaluate(root, NULL, "startup", &report),
               "expected oversized-token evaluation to succeed")) return 1;
    if (artifact_has_state(&report, "tokens", "pruned", "artifact_oversized_load_cap_pruned")) return 1;
    const AnalysisCacheManifestArtifact* token_artifact =
        analysis_cache_manifest_find_artifact(&report, "tokens");
    if (expect(token_artifact != NULL, "expected token artifact after prune")) return 1;
    if (expect(!token_artifact->present, "expected pruned token artifact to be absent")) return 1;
    if (expect(token_artifact->size_bytes == 0, "expected pruned token artifact size to be zero")) return 1;
    if (expect(access(path, F_OK) != 0, "expected oversized token artifact to be pruned")) return 1;

    if (expect(analysis_cache_manifest_evaluate(root, "-DCHANGED", "startup", &report),
               "expected mismatched-meta evaluation to succeed")) return 1;
    if (expect(!report.cache_meta_trusted, "expected changed build args to reject meta")) return 1;
    if (expect(strcmp(report.cache_meta_reason, "cache_meta_build_args_changed") == 0,
               "expected build args changed reason")) return 1;
    if (artifact_has_state(&report, "diagnostics", "ignored", "cache_meta_untrusted")) return 1;

    if (expect(analysis_cache_manifest_save(&report), "expected manifest save")) return 1;
    snprintf(path, sizeof(path), "%s/ide_files/cache_manifest.json", root);
    struct stat st;
    if (expect(stat(path, &st) == 0 && st.st_size > 0, "expected manifest file")) return 1;

    printf("analysis_cache_manifest_test: ok\n");
    return 0;
}
