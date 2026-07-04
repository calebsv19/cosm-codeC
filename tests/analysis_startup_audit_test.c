#include "core/Analysis/analysis_startup_audit.h"

#include <stdio.h>
#include <string.h>

#include "core/Analysis/analysis_cache.h"
#include "core/Analysis/analysis_snapshot.h"
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

static int make_workspace(char* root, size_t root_cap, const char* name) {
    char fixture_name[128];
    snprintf(fixture_name, sizeof(fixture_name), "ide_startup_audit_%s", name);
    if (!ide_test_fixture_root(root, root_cap, fixture_name)) return 1;
    char path[512];
    if (!ide_test_ensure_dir(root)) return 1;
    snprintf(path, sizeof(path), "%s/src", root);
    if (!ide_test_ensure_dir(path)) return 1;
    snprintf(path, sizeof(path), "%s/include", root);
    if (!ide_test_ensure_dir(path)) return 1;
    snprintf(path, sizeof(path), "%s/ide_files", root);
    if (!ide_test_ensure_dir(path)) return 1;
    snprintf(path, sizeof(path), "%s/src/main.c", root);
    if (!ide_test_write_text_file(path, "#include \"a.h\"\nint main(void){return VALUE;}\n")) return 1;
    snprintf(path, sizeof(path), "%s/include/a.h", root);
    if (!ide_test_write_text_file(path, "#define VALUE 0\n")) return 1;
    return 0;
}

static int write_artifact(const char* root, const char* name) {
    char path[512];
    snprintf(path, sizeof(path), "%s/ide_files/%s", root, name);
    return ide_test_write_text_file(path, "[]") ? 0 : 1;
}

static int write_required_artifacts(const char* root, int include_graph) {
    if (write_artifact(root, "analysis_diagnostics.json") != 0) return 1;
    if (write_artifact(root, "analysis_symbols.json") != 0) return 1;
    if (write_artifact(root, "analysis_units_attachments.json") != 0) return 1;
    if (write_artifact(root, "library_index.json") != 0) return 1;
    if (write_artifact(root, "build_flags.json") != 0) return 1;
    if (include_graph && write_artifact(root, "include_graph.json") != 0) return 1;
    return 0;
}

static int save_snapshot(const char* root) {
    AnalysisSnapshot snapshot = {0};
    analysis_snapshot_init(&snapshot);
    int failed = 0;
    if (!analysis_snapshot_scan_workspace(root, &snapshot) ||
        !analysis_snapshot_save(root, &snapshot)) {
        failed = 1;
    }
    analysis_snapshot_clear(&snapshot);
    return failed;
}

static int setup_trusted_workspace(char* root,
                                   size_t root_cap,
                                   const char* name,
                                   int include_graph) {
    if (make_workspace(root, root_cap, name) != 0) return 1;
    if (!analysis_cache_save_metadata(root, NULL)) return 1;
    if (save_snapshot(root) != 0) return 1;
    if (write_required_artifacts(root, include_graph) != 0) return 1;
    return 0;
}

int main(void) {
    AnalysisStartupAudit audit;

    char missing_root[256];
    if (expect(make_workspace(missing_root, sizeof(missing_root), "missing") == 0,
               "expected missing-meta workspace")) return 1;
    if (expect(analysis_startup_audit_evaluate(missing_root, NULL, &audit),
               "expected missing-meta audit")) return 1;
    if (expect(!audit.cache_meta_trusted, "expected missing meta untrusted")) return 1;
    if (expect(audit.refresh_intent == ANALYSIS_STARTUP_REFRESH_FULL_REQUIRED,
               "expected missing meta full intent")) return 1;
    if (expect(strcmp(audit.refresh_reason, "cache_meta_missing") == 0,
               "expected missing meta reason")) return 1;

    char clean_root[256];
    if (expect(setup_trusted_workspace(clean_root, sizeof(clean_root), "clean", 1) == 0,
               "expected clean workspace setup")) return 1;
    if (expect(analysis_startup_audit_evaluate(clean_root, NULL, &audit),
               "expected clean audit")) return 1;
    if (expect(audit.cache_meta_trusted, "expected trusted cache meta")) return 1;
    if (expect(audit.current_file_count == 2, "expected two current files")) return 1;
    if (expect(audit.cached_file_count == 2, "expected two cached files")) return 1;
    if (expect(audit.hash_matched_count == 2, "expected two hash matches")) return 1;
    if (expect(audit.dirty_count == 0, "expected clean dirty count")) return 1;
    if (expect(!audit.tokens_ready, "expected missing tokens to be optional")) return 1;
    if (expect(!audit.tokens_required, "expected tokens not required")) return 1;
    if (expect(audit.refresh_intent == ANALYSIS_STARTUP_REFRESH_INCREMENTAL_VERIFY_NOOP,
               "expected clean verify intent")) return 1;

    char changed_path[512];
    snprintf(changed_path, sizeof(changed_path), "%s/src/main.c", clean_root);
    if (expect(ide_test_write_text_file(changed_path, "#include \"a.h\"\nint main(void){return 7;}\n"),
               "expected source change")) return 1;
    if (expect(analysis_startup_audit_evaluate(clean_root, NULL, &audit),
               "expected changed-source audit")) return 1;
    if (expect(audit.dirty_count == 1, "expected one dirty file")) return 1;
    if (expect(audit.new_count == 0, "expected no new files")) return 1;
    if (expect(audit.refresh_intent == ANALYSIS_STARTUP_REFRESH_INCREMENTAL_TARGETS,
               "expected incremental target intent")) return 1;

    char header_root[256];
    if (expect(setup_trusted_workspace(header_root, sizeof(header_root), "header", 0) == 0,
               "expected header workspace setup")) return 1;
    snprintf(changed_path, sizeof(changed_path), "%s/include/a.h", header_root);
    if (expect(ide_test_write_text_file(changed_path, "#define VALUE 9\n"),
               "expected header change")) return 1;
    if (expect(analysis_startup_audit_evaluate(header_root, NULL, &audit),
               "expected header audit")) return 1;
    if (expect(audit.dirty_count == 1, "expected one dirty header")) return 1;
    if (expect(!audit.include_graph_ready, "expected include graph unavailable")) return 1;
    if (expect(audit.refresh_intent == ANALYSIS_STARTUP_REFRESH_FULL_REQUIRED,
               "expected header full intent")) return 1;
    if (expect(strcmp(audit.refresh_reason, "header_change_without_include_graph") == 0,
               "expected header graph reason")) return 1;

    printf("analysis_startup_audit_test: ok\n");
    return 0;
}
