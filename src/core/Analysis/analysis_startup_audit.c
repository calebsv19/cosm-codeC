#include "core/Analysis/analysis_startup_audit.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "core/Analysis/analysis_cache.h"
#include "core/Analysis/analysis_cache_manifest.h"
#include "core/Analysis/analysis_snapshot.h"

typedef struct {
    const char* relative_path;
    long long max_trusted_size;
} StartupAuditArtifactSpec;

static void copy_text(char* dst, size_t dst_size, const char* src) {
    if (!dst || dst_size == 0) return;
    snprintf(dst, dst_size, "%s", src ? src : "");
    dst[dst_size - 1] = '\0';
}

void analysis_startup_audit_init(AnalysisStartupAudit* audit) {
    if (!audit) return;
    memset(audit, 0, sizeof(*audit));
    audit->refresh_intent = ANALYSIS_STARTUP_REFRESH_UNKNOWN;
    audit->tokens_required = false;
}

const char* analysis_startup_refresh_intent_string(AnalysisStartupRefreshIntent intent) {
    switch (intent) {
        case ANALYSIS_STARTUP_REFRESH_FULL_REQUIRED: return "full_required";
        case ANALYSIS_STARTUP_REFRESH_INCREMENTAL_TARGETS: return "incremental_targets";
        case ANALYSIS_STARTUP_REFRESH_INCREMENTAL_VERIFY_NOOP: return "incremental_verify_noop";
        case ANALYSIS_STARTUP_REFRESH_UNKNOWN:
        default: return "unknown";
    }
}

static bool stat_artifact(const char* workspace_root,
                          const StartupAuditArtifactSpec* spec) {
    if (!workspace_root || !*workspace_root || !spec || !spec->relative_path) {
        return false;
    }
    char path[2048];
    snprintf(path, sizeof(path), "%s/%s", workspace_root, spec->relative_path);
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
        return false;
    }
    if (spec->max_trusted_size > 0 && (long long)st.st_size > spec->max_trusted_size) {
        return false;
    }
    return true;
}

static bool artifact_ready(const char* workspace_root,
                           bool cache_meta_trusted,
                           const char* relative_path,
                           long long max_trusted_size) {
    if (!cache_meta_trusted) return false;
    StartupAuditArtifactSpec spec = {
        .relative_path = relative_path,
        .max_trusted_size = max_trusted_size
    };
    return stat_artifact(workspace_root, &spec);
}

static const AnalysisFileFingerprint* find_fingerprint(const AnalysisSnapshot* snapshot,
                                                       const char* path) {
    if (!snapshot || !path || !*path) return NULL;
    for (size_t i = 0; i < snapshot->file_count; ++i) {
        const AnalysisFileFingerprint* f = &snapshot->files[i];
        if (f->path && strcmp(f->path, path) == 0) {
            return f;
        }
    }
    return NULL;
}

static bool fingerprints_match(const AnalysisFileFingerprint* cached,
                               const AnalysisFileFingerprint* current) {
    if (!cached || !current) return false;
    if (cached->content_hash != 0 && current->content_hash != 0) {
        return cached->content_hash == current->content_hash;
    }
    return cached->mtime == current->mtime && cached->size == current->size;
}

static bool has_header_extension(const char* path) {
    if (!path) return false;
    size_t len = strlen(path);
    return len >= 2 && strcmp(path + len - 2, ".h") == 0;
}

static bool path_list_has_header(char* const* paths, size_t count) {
    if (!paths) return false;
    for (size_t i = 0; i < count; ++i) {
        if (has_header_extension(paths[i])) {
            return true;
        }
    }
    return false;
}

static void evaluate_cache_meta(const char* workspace_root,
                                const char* build_args,
                                AnalysisStartupAudit* audit) {
    AnalysisCacheMeta meta = {0};
    if (!analysis_cache_load_meta(&meta, workspace_root)) {
        audit->cache_meta_trusted = false;
        copy_text(audit->cache_meta_reason,
                  sizeof(audit->cache_meta_reason),
                  "cache_meta_missing");
        return;
    }

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

    audit->cache_meta_trusted = strcmp(reason, "accepted") == 0;
    copy_text(audit->cache_meta_reason, sizeof(audit->cache_meta_reason), reason);
}

static void evaluate_artifacts(const char* workspace_root, AnalysisStartupAudit* audit) {
    bool trusted = audit && audit->cache_meta_trusted;
    if (!audit) return;
    audit->diagnostics_ready =
        artifact_ready(workspace_root, trusted, "ide_files/analysis_diagnostics.json", 0);
    audit->symbols_ready =
        artifact_ready(workspace_root, trusted, "ide_files/analysis_symbols.json", 0);
    audit->units_ready =
        artifact_ready(workspace_root, trusted, "ide_files/analysis_units_attachments.json", 0);
    audit->tokens_ready =
        artifact_ready(workspace_root,
                       trusted,
                       "ide_files/analysis_tokens.json",
                       ANALYSIS_CACHE_TOKEN_LOAD_LIMIT_BYTES);
    audit->library_ready =
        artifact_ready(workspace_root, trusted, "ide_files/library_index.json", 0);
    audit->include_graph_ready =
        artifact_ready(workspace_root, trusted, "ide_files/include_graph.json", 0);
    audit->build_flags_ready =
        artifact_ready(workspace_root, trusted, "ide_files/build_flags.json", 0);
}

static void set_intent(AnalysisStartupAudit* audit,
                       AnalysisStartupRefreshIntent intent,
                       const char* reason) {
    if (!audit) return;
    audit->refresh_intent = intent;
    copy_text(audit->refresh_reason, sizeof(audit->refresh_reason), reason);
}

bool analysis_startup_audit_evaluate(const char* workspace_root,
                                     const char* build_args,
                                     AnalysisStartupAudit* out_audit) {
    if (!workspace_root || !*workspace_root || !out_audit) return false;
    analysis_startup_audit_init(out_audit);

    evaluate_cache_meta(workspace_root, build_args, out_audit);
    evaluate_artifacts(workspace_root, out_audit);

    AnalysisSnapshot cached = {0};
    AnalysisSnapshot current = {0};
    analysis_snapshot_init(&cached);
    analysis_snapshot_init(&current);

    out_audit->snapshot_loaded = analysis_snapshot_load(workspace_root, &cached);
    out_audit->current_scan_ok = analysis_snapshot_scan_workspace(workspace_root, &current);
    out_audit->cached_file_count = out_audit->snapshot_loaded ? cached.file_count : 0;
    out_audit->current_file_count = out_audit->current_scan_ok ? current.file_count : 0;

    if (out_audit->snapshot_loaded && out_audit->current_scan_ok) {
        char** dirty_paths = NULL;
        char** removed_paths = NULL;
        size_t dirty_count = 0;
        size_t removed_count = 0;
        if (analysis_snapshot_compute_dirty_sets(&cached,
                                                 &current,
                                                 &dirty_paths,
                                                 &dirty_count,
                                                 &removed_paths,
                                                 &removed_count)) {
            out_audit->dirty_count = dirty_count;
            out_audit->removed_count = removed_count;
            for (size_t i = 0; i < current.file_count; ++i) {
                const AnalysisFileFingerprint* cur = &current.files[i];
                const AnalysisFileFingerprint* old = find_fingerprint(&cached, cur->path);
                if (!old) {
                    out_audit->new_count++;
                } else if (fingerprints_match(old, cur)) {
                    out_audit->hash_matched_count++;
                }
            }
            bool header_change = path_list_has_header(dirty_paths, dirty_count) ||
                                 path_list_has_header(removed_paths, removed_count);
            if (!out_audit->cache_meta_trusted) {
                set_intent(out_audit,
                           ANALYSIS_STARTUP_REFRESH_FULL_REQUIRED,
                           out_audit->cache_meta_reason);
            } else if (!out_audit->diagnostics_ready ||
                       !out_audit->symbols_ready ||
                       !out_audit->library_ready) {
                set_intent(out_audit,
                           ANALYSIS_STARTUP_REFRESH_FULL_REQUIRED,
                           "required_cache_artifact_missing");
            } else if (header_change && !out_audit->include_graph_ready) {
                set_intent(out_audit,
                           ANALYSIS_STARTUP_REFRESH_FULL_REQUIRED,
                           "header_change_without_include_graph");
            } else if (dirty_count > 0 || removed_count > 0) {
                set_intent(out_audit,
                           ANALYSIS_STARTUP_REFRESH_INCREMENTAL_TARGETS,
                           "source_hash_changes_detected");
            } else {
                set_intent(out_audit,
                           ANALYSIS_STARTUP_REFRESH_INCREMENTAL_VERIFY_NOOP,
                           "source_hashes_match_startup_verify_scheduled");
            }
        } else {
            set_intent(out_audit,
                       ANALYSIS_STARTUP_REFRESH_FULL_REQUIRED,
                       "snapshot_diff_failed");
        }
        analysis_snapshot_free_path_list(dirty_paths, dirty_count);
        analysis_snapshot_free_path_list(removed_paths, removed_count);
    } else if (!out_audit->cache_meta_trusted) {
        set_intent(out_audit,
                   ANALYSIS_STARTUP_REFRESH_FULL_REQUIRED,
                   out_audit->cache_meta_reason);
    } else if (!out_audit->current_scan_ok) {
        set_intent(out_audit,
                   ANALYSIS_STARTUP_REFRESH_FULL_REQUIRED,
                   "current_source_scan_failed");
    } else {
        set_intent(out_audit,
                   ANALYSIS_STARTUP_REFRESH_FULL_REQUIRED,
                   "source_index_missing_or_invalid");
    }

    out_audit->valid = true;
    analysis_snapshot_clear(&cached);
    analysis_snapshot_clear(&current);
    return true;
}
