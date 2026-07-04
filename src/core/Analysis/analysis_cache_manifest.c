#include "core/Analysis/analysis_cache_manifest.h"

#include <json-c/json.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "core/Analysis/analysis_cache.h"
#include "core/Analysis/analysis_artifact_io.h"

typedef struct {
    const char* name;
    const char* relative_path;
    long long max_trusted_size;
    bool prune_if_oversized;
} ArtifactSpec;

static const ArtifactSpec kArtifactSpecs[] = {
    {"cache_meta", "ide_files/cache_meta.json", 0, false},
    {"source_index", "ide_files/index.json", 0, false},
    {"diagnostics", "ide_files/analysis_diagnostics.json", 0, false},
    {"symbols", "ide_files/analysis_symbols.json", 0, false},
    {"units", "ide_files/analysis_units_attachments.json", 0, false},
    {"tokens", "ide_files/analysis_tokens.json", ANALYSIS_CACHE_TOKEN_LOAD_LIMIT_BYTES, true},
    {"include_graph", "ide_files/include_graph.json", 0, false},
    {"library_index", "ide_files/library_index.json", 0, false},
    {"build_flags", "ide_files/build_flags.json", 0, false},
};

static void copy_text(char* dst, size_t dst_size, const char* src) {
    if (!dst || dst_size == 0) return;
    snprintf(dst, dst_size, "%s", src ? src : "");
    dst[dst_size - 1] = '\0';
}

static bool stat_artifact(const char* workspace_root,
                          const char* relative_path,
                          long long* out_size) {
    if (out_size) *out_size = 0;
    if (!workspace_root || !*workspace_root || !relative_path || !*relative_path) return false;
    char path[2048];
    snprintf(path, sizeof(path), "%s/%s", workspace_root, relative_path);
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) return false;
    if (out_size) *out_size = (long long)st.st_size;
    return true;
}

static void artifact_path(const char* workspace_root,
                          const char* relative_path,
                          char* out_path,
                          size_t out_path_size) {
    if (!out_path || out_path_size == 0) return;
    snprintf(out_path,
             out_path_size,
             "%s/%s",
             workspace_root ? workspace_root : "",
             relative_path ? relative_path : "");
    out_path[out_path_size - 1] = '\0';
}

static void evaluate_cache_meta(const char* workspace_root,
                                const char* build_args,
                                AnalysisCacheManifestReport* report) {
    AnalysisCacheMeta meta = {0};
    if (!analysis_cache_load_meta(&meta, workspace_root)) {
        report->cache_meta_present = false;
        report->cache_meta_trusted = false;
        copy_text(report->cache_meta_state, sizeof(report->cache_meta_state), "missing");
        copy_text(report->cache_meta_reason, sizeof(report->cache_meta_reason), "cache_meta_missing");
        return;
    }

    report->cache_meta_present = true;
    AnalysisCacheMeta current = {0};
    analysis_cache_compute_meta(workspace_root, build_args, &current);

    const char* reason = "accepted";
    if (meta.version != ANALYSIS_CACHE_VERSION) {
        reason = "cache_meta_version_mismatch";
    } else if (strcmp(meta.project_root, current.project_root) != 0) {
        reason = "cache_meta_project_root_mismatch";
    } else if (meta.build_args_hash != current.build_args_hash) {
        reason = "cache_meta_build_args_changed";
    } else if (meta.makefile_mtime != current.makefile_mtime) {
        reason = "cache_meta_makefile_changed";
    } else if (meta.frontend_fingerprint != current.frontend_fingerprint) {
        reason = "cache_meta_frontend_fingerprint_changed";
    } else if (strcmp(meta.frontend_lib_path, current.frontend_lib_path) != 0) {
        reason = "cache_meta_frontend_path_changed";
    }

    report->cache_meta_trusted = strcmp(reason, "accepted") == 0;
    copy_text(report->cache_meta_state,
              sizeof(report->cache_meta_state),
              report->cache_meta_trusted ? "accepted" : "rejected");
    copy_text(report->cache_meta_reason, sizeof(report->cache_meta_reason), reason);
}

static void add_artifact_report(AnalysisCacheManifestReport* report,
                                const char* workspace_root,
                                const ArtifactSpec* spec) {
    if (!report || !spec || report->artifact_count >= ANALYSIS_CACHE_MANIFEST_MAX_ARTIFACTS) return;
    AnalysisCacheManifestArtifact* artifact = &report->artifacts[report->artifact_count++];
    memset(artifact, 0, sizeof(*artifact));
    copy_text(artifact->name, sizeof(artifact->name), spec->name);
    copy_text(artifact->relative_path, sizeof(artifact->relative_path), spec->relative_path);

    long long size = 0;
    artifact->present = stat_artifact(workspace_root, spec->relative_path, &size);
    artifact->size_bytes = artifact->present ? size : 0;

    if (!artifact->present) {
        copy_text(artifact->trust_state, sizeof(artifact->trust_state), "missing");
        copy_text(artifact->reason, sizeof(artifact->reason), "artifact_missing");
    } else if (!report->cache_meta_trusted) {
        copy_text(artifact->trust_state, sizeof(artifact->trust_state), "ignored");
        copy_text(artifact->reason, sizeof(artifact->reason), "cache_meta_untrusted");
    } else if (spec->max_trusted_size > 0 && artifact->size_bytes > spec->max_trusted_size) {
        if (spec->prune_if_oversized) {
            char path[2048];
            artifact_path(workspace_root, spec->relative_path, path, sizeof(path));
            if (unlink(path) == 0 || access(path, F_OK) != 0) {
                artifact->present = false;
                artifact->size_bytes = 0;
                copy_text(artifact->trust_state, sizeof(artifact->trust_state), "pruned");
                copy_text(artifact->reason,
                          sizeof(artifact->reason),
                          "artifact_oversized_load_cap_pruned");
            } else {
                copy_text(artifact->trust_state, sizeof(artifact->trust_state), "rejected");
                copy_text(artifact->reason,
                          sizeof(artifact->reason),
                          "artifact_oversized_load_cap_prune_failed");
            }
        } else {
            copy_text(artifact->trust_state, sizeof(artifact->trust_state), "rejected");
            copy_text(artifact->reason, sizeof(artifact->reason), "artifact_oversized_load_cap");
        }
    } else {
        copy_text(artifact->trust_state, sizeof(artifact->trust_state), "accepted");
        copy_text(artifact->reason, sizeof(artifact->reason), "accepted");
    }
}

bool analysis_cache_manifest_evaluate(const char* workspace_root,
                                      const char* build_args,
                                      const char* phase,
                                      AnalysisCacheManifestReport* out_report) {
    if (!workspace_root || !*workspace_root || !out_report) return false;
    memset(out_report, 0, sizeof(*out_report));
    out_report->version = ANALYSIS_CACHE_MANIFEST_VERSION;
    copy_text(out_report->project_root, sizeof(out_report->project_root), workspace_root);
    copy_text(out_report->phase, sizeof(out_report->phase), phase ? phase : "unknown");
    out_report->generated_unix_time = (long long)time(NULL);

    evaluate_cache_meta(workspace_root, build_args, out_report);
    if (phase && strcmp(phase, "startup") == 0) {
        out_report->has_startup_audit =
            analysis_startup_audit_evaluate(workspace_root,
                                            build_args,
                                            &out_report->startup_audit);
    }
    for (size_t i = 0; i < sizeof(kArtifactSpecs) / sizeof(kArtifactSpecs[0]); ++i) {
        add_artifact_report(out_report, workspace_root, &kArtifactSpecs[i]);
    }
    return true;
}

static json_object* startup_audit_to_json(const AnalysisStartupAudit* audit) {
    json_object* obj = json_object_new_object();
    if (!obj) return NULL;
    if (!audit) return obj;

    json_object_object_add(obj, "valid", json_object_new_boolean(audit->valid));
    json_object_object_add(obj,
                           "cache_meta_trusted",
                           json_object_new_boolean(audit->cache_meta_trusted));
    json_object_object_add(obj,
                           "cache_meta_reason",
                           json_object_new_string(audit->cache_meta_reason));
    json_object_object_add(obj, "snapshot_loaded", json_object_new_boolean(audit->snapshot_loaded));
    json_object_object_add(obj, "current_scan_ok", json_object_new_boolean(audit->current_scan_ok));
    json_object_object_add(obj,
                           "current_file_count",
                           json_object_new_int64((long long)audit->current_file_count));
    json_object_object_add(obj,
                           "cached_file_count",
                           json_object_new_int64((long long)audit->cached_file_count));
    json_object_object_add(obj,
                           "hash_matched_count",
                           json_object_new_int64((long long)audit->hash_matched_count));
    json_object_object_add(obj,
                           "dirty_count",
                           json_object_new_int64((long long)audit->dirty_count));
    json_object_object_add(obj,
                           "removed_count",
                           json_object_new_int64((long long)audit->removed_count));
    json_object_object_add(obj,
                           "new_count",
                           json_object_new_int64((long long)audit->new_count));

    json_object* artifacts = json_object_new_object();
    json_object_object_add(artifacts,
                           "diagnostics_ready",
                           json_object_new_boolean(audit->diagnostics_ready));
    json_object_object_add(artifacts, "symbols_ready", json_object_new_boolean(audit->symbols_ready));
    json_object_object_add(artifacts, "units_ready", json_object_new_boolean(audit->units_ready));
    json_object_object_add(artifacts, "tokens_ready", json_object_new_boolean(audit->tokens_ready));
    json_object_object_add(artifacts,
                           "tokens_required",
                           json_object_new_boolean(audit->tokens_required));
    json_object_object_add(artifacts, "library_ready", json_object_new_boolean(audit->library_ready));
    json_object_object_add(artifacts,
                           "include_graph_ready",
                           json_object_new_boolean(audit->include_graph_ready));
    json_object_object_add(artifacts,
                           "build_flags_ready",
                           json_object_new_boolean(audit->build_flags_ready));
    json_object_object_add(obj, "artifacts", artifacts);

    json_object_object_add(obj,
                           "refresh_intent",
                           json_object_new_string(
                               analysis_startup_refresh_intent_string(audit->refresh_intent)));
    json_object_object_add(obj, "refresh_reason", json_object_new_string(audit->refresh_reason));
    return obj;
}

bool analysis_cache_manifest_save(const AnalysisCacheManifestReport* report) {
    if (!report || !report->project_root[0]) return false;

    json_object* root = json_object_new_object();
    json_object_object_add(root, "version", json_object_new_int((int)report->version));
    json_object_object_add(root, "project_root", json_object_new_string(report->project_root));
    json_object_object_add(root, "phase", json_object_new_string(report->phase));
    json_object_object_add(root, "generated_unix_time",
                           json_object_new_int64((long long)report->generated_unix_time));

    json_object* meta = json_object_new_object();
    json_object_object_add(meta, "present", json_object_new_boolean(report->cache_meta_present));
    json_object_object_add(meta, "trusted", json_object_new_boolean(report->cache_meta_trusted));
    json_object_object_add(meta, "state", json_object_new_string(report->cache_meta_state));
    json_object_object_add(meta, "reason", json_object_new_string(report->cache_meta_reason));
    json_object_object_add(root, "cache_meta", meta);

    if (report->has_startup_audit) {
        json_object* audit = startup_audit_to_json(&report->startup_audit);
        if (audit) {
            json_object_object_add(root, "startup_audit", audit);
        }
    }

    json_object* artifacts = json_object_new_array();
    for (size_t i = 0; i < report->artifact_count; ++i) {
        const AnalysisCacheManifestArtifact* artifact = &report->artifacts[i];
        json_object* item = json_object_new_object();
        json_object_object_add(item, "name", json_object_new_string(artifact->name));
        json_object_object_add(item, "relative_path", json_object_new_string(artifact->relative_path));
        json_object_object_add(item, "present", json_object_new_boolean(artifact->present));
        json_object_object_add(item, "size_bytes",
                               json_object_new_int64((long long)artifact->size_bytes));
        json_object_object_add(item, "trust_state", json_object_new_string(artifact->trust_state));
        json_object_object_add(item, "reason", json_object_new_string(artifact->reason));
        json_object_array_add(artifacts, item);
    }
    json_object_object_add(root, "artifacts", artifacts);

    const char* serialized = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PRETTY);
    bool ok = serialized &&
              analysis_artifact_io_write_text(report->project_root,
                                              "cache_manifest.json",
                                              serialized);
    json_object_put(root);
    return ok;
}

bool analysis_cache_manifest_write_report(const char* workspace_root,
                                          const char* build_args,
                                          const char* phase) {
    AnalysisCacheManifestReport report;
    if (!analysis_cache_manifest_evaluate(workspace_root, build_args, phase, &report)) {
        return false;
    }
    return analysis_cache_manifest_save(&report);
}

const AnalysisCacheManifestArtifact*
analysis_cache_manifest_find_artifact(const AnalysisCacheManifestReport* report,
                                      const char* name) {
    if (!report || !name || !*name) return NULL;
    for (size_t i = 0; i < report->artifact_count; ++i) {
        if (strcmp(report->artifacts[i].name, name) == 0) {
            return &report->artifacts[i];
        }
    }
    return NULL;
}
