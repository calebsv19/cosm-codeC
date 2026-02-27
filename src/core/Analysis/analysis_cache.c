#include "core/Analysis/analysis_cache.h"

#include <json-c/json.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "core/Analysis/analysis_store.h"
#include "core/Analysis/analysis_symbols_store.h"
#include "core/Analysis/analysis_token_store.h"
#include "core/Analysis/library_index.h"

static void ensure_cache_dir(const char* workspace_root) {
    if (!workspace_root || !*workspace_root) return;
    char dir[PATH_MAX];
    snprintf(dir, sizeof(dir), "%s/ide_files", workspace_root);
    struct stat st;
    if (stat(dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
        mkdir(dir, 0755);
    }
}

static uint64_t fnv1a64(const char* s, uint64_t seed) {
    uint64_t hash = seed ? seed : 0xcbf29ce484222325ULL;
    const unsigned char* p = (const unsigned char*)s;
    while (p && *p) {
        hash ^= (uint64_t)(*p++);
        hash *= 0x100000001b3ULL;
    }
    return hash;
}

static long get_makefile_mtime(const char* workspace_root) {
    if (!workspace_root || !*workspace_root) return 0;
    char path[PATH_MAX];
    struct stat st;
    snprintf(path, sizeof(path), "%s/Makefile", workspace_root);
    if (stat(path, &st) == 0) return st.st_mtime;
    snprintf(path, sizeof(path), "%s/makefile", workspace_root);
    if (stat(path, &st) == 0) return st.st_mtime;
    return 0;
}

static bool resolve_frontend_lib_candidate(const char* candidate,
                                           char* out_path,
                                           size_t out_path_size,
                                           uint64_t* out_fp) {
    if (!candidate || !candidate[0] || !out_path || out_path_size == 0 || !out_fp) {
        return false;
    }

    struct stat st;
    if (stat(candidate, &st) != 0 || !S_ISREG(st.st_mode)) {
        return false;
    }

    char resolved[PATH_MAX];
    const char* chosen = candidate;
    if (realpath(candidate, resolved)) {
        chosen = resolved;
    }

    snprintf(out_path, out_path_size, "%s", chosen);
    out_path[out_path_size - 1] = '\0';

    uint64_t fp = fnv1a64(out_path, 0);
    char sig[96];
    snprintf(sig, sizeof(sig), "%lld:%lld",
             (long long)st.st_mtime,
             (long long)st.st_size);
    fp = fnv1a64(sig, fp);
    *out_fp = fp;
    return true;
}

static void compute_frontend_lib_fingerprint(AnalysisCacheMeta* out) {
    if (!out) return;

    out->frontend_fingerprint = 0;
    out->frontend_lib_path[0] = '\0';

    const char* override_path = getenv("IDE_FISICS_FRONTEND_LIB");
    if (resolve_frontend_lib_candidate(override_path,
                                       out->frontend_lib_path,
                                       sizeof(out->frontend_lib_path),
                                       &out->frontend_fingerprint)) {
        return;
    }

    const char* candidates[] = {
        "../fisiCs/libfisics_frontend_unsanitized.a",
        "../fisiCs/libfisics_frontend_sanitized.a",
        "../fisics/libfisics_frontend_unsanitized.a",
        "../fisics/libfisics_frontend_sanitized.a"
    };

    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
        if (resolve_frontend_lib_candidate(candidates[i],
                                           out->frontend_lib_path,
                                           sizeof(out->frontend_lib_path),
                                           &out->frontend_fingerprint)) {
            return;
        }
    }
}

void analysis_cache_compute_meta(const char* workspace_root,
                                 const char* build_args,
                                 AnalysisCacheMeta* out) {
    if (!out) return;
    memset(out, 0, sizeof(*out));
    out->version = ANALYSIS_CACHE_VERSION;
    if (workspace_root && *workspace_root) {
        strncpy(out->project_root, workspace_root, sizeof(out->project_root) - 1);
        out->project_root[sizeof(out->project_root) - 1] = '\0';
    }
    uint64_t h = fnv1a64(out->project_root, 0);
    if (build_args && *build_args) {
        h = fnv1a64(build_args, h);
    }
    out->build_args_hash = h;
    out->makefile_mtime = get_makefile_mtime(workspace_root);
    compute_frontend_lib_fingerprint(out);
}

bool analysis_cache_save_meta(const AnalysisCacheMeta* meta, const char* workspace_root) {
    if (!meta || !workspace_root || !*workspace_root) return false;
    ensure_cache_dir(workspace_root);
    json_object* root = json_object_new_object();
    json_object_object_add(root, "version", json_object_new_int((int)meta->version));
    json_object_object_add(root, "build_args_hash",
                           json_object_new_int64((long long)meta->build_args_hash));
    json_object_object_add(root, "frontend_fingerprint",
                           json_object_new_int64((long long)meta->frontend_fingerprint));
    json_object_object_add(root, "makefile_mtime",
                           json_object_new_int64((long long)meta->makefile_mtime));
    json_object_object_add(root, "project_root",
                           json_object_new_string(meta->project_root[0] ? meta->project_root : ""));
    json_object_object_add(root, "frontend_lib_path",
                           json_object_new_string(meta->frontend_lib_path[0] ? meta->frontend_lib_path : ""));
    const char* serialized = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/ide_files/cache_meta.json", workspace_root);
    FILE* f = fopen(path, "w");
    if (f && serialized) {
        fputs(serialized, f);
        fclose(f);
        json_object_put(root);
        return true;
    }
    if (f) fclose(f);
    json_object_put(root);
    return false;
}

bool analysis_cache_load_meta(AnalysisCacheMeta* out, const char* workspace_root) {
    if (!out || !workspace_root || !*workspace_root) return false;
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/ide_files/cache_meta.json", workspace_root);
    FILE* f = fopen(path, "r");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0 || len > (32 * 1024 * 1024)) {
        fclose(f);
        return false;
    }
    char* buf = malloc((size_t)len + 1);
    if (!buf) {
        fclose(f);
        return false;
    }
    fread(buf, 1, (size_t)len, f);
    buf[len] = '\0';
    fclose(f);

    json_object* root = json_tokener_parse(buf);
    free(buf);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        return false;
    }

    AnalysisCacheMeta meta = {0};
    json_object* jv = NULL;
    json_object* jh = NULL;
    json_object* jf = NULL;
    json_object* jm = NULL;
    json_object* jp = NULL;
    json_object* jl = NULL;
    json_object_object_get_ex(root, "version", &jv);
    json_object_object_get_ex(root, "build_args_hash", &jh);
    json_object_object_get_ex(root, "frontend_fingerprint", &jf);
    json_object_object_get_ex(root, "makefile_mtime", &jm);
    json_object_object_get_ex(root, "project_root", &jp);
    json_object_object_get_ex(root, "frontend_lib_path", &jl);
    meta.version = jv ? (uint32_t)json_object_get_int(jv) : 0;
    meta.build_args_hash = jh ? (uint64_t)json_object_get_int64(jh) : 0;
    meta.frontend_fingerprint = jf ? (uint64_t)json_object_get_int64(jf) : 0;
    meta.makefile_mtime = jm ? (long)json_object_get_int64(jm) : 0;
    if (jp) {
        const char* pr = json_object_get_string(jp);
        if (pr) {
            strncpy(meta.project_root, pr, sizeof(meta.project_root) - 1);
            meta.project_root[sizeof(meta.project_root) - 1] = '\0';
        }
    }
    if (jl) {
        const char* lib_path = json_object_get_string(jl);
        if (lib_path) {
            strncpy(meta.frontend_lib_path, lib_path, sizeof(meta.frontend_lib_path) - 1);
            meta.frontend_lib_path[sizeof(meta.frontend_lib_path) - 1] = '\0';
        }
    }
    json_object_put(root);
    *out = meta;
    return true;
}

bool analysis_cache_meta_matches(const AnalysisCacheMeta* meta,
                                 const char* workspace_root,
                                 const char* build_args) {
    if (!meta || meta->version != ANALYSIS_CACHE_VERSION) return false;
    if (!workspace_root || !*workspace_root) return false;
    AnalysisCacheMeta current = {0};
    analysis_cache_compute_meta(workspace_root, build_args, &current);
    if (strcmp(meta->project_root, current.project_root) != 0) return false;
    if (meta->build_args_hash != current.build_args_hash) return false;
    if (meta->makefile_mtime != current.makefile_mtime) return false;
    if (meta->frontend_fingerprint != current.frontend_fingerprint) return false;
    if (strcmp(meta->frontend_lib_path, current.frontend_lib_path) != 0) return false;
    return true;
}

bool analysis_cache_save_metadata(const char* workspace_root, const char* build_args) {
    AnalysisCacheMeta meta = {0};
    analysis_cache_compute_meta(workspace_root, build_args, &meta);
    return analysis_cache_save_meta(&meta, workspace_root);
}

// Artifact helpers ----------------------------------------------------------

bool analysis_cache_save_errors(const char* workspace_root) {
    if (!workspace_root || !*workspace_root) return false;
    analysis_store_save(workspace_root);
    return true;
}

bool analysis_cache_load_errors(const char* workspace_root, const char* build_args) {
    if (!workspace_root || !*workspace_root) return false;
    AnalysisCacheMeta meta = {0};
    if (!analysis_cache_load_meta(&meta, workspace_root) ||
        !analysis_cache_meta_matches(&meta, workspace_root, build_args)) {
        return false;
    }
    analysis_store_load(workspace_root);
    return true;
}

bool analysis_cache_save_symbols(const char* workspace_root) {
    if (!workspace_root || !*workspace_root) return false;
    analysis_symbols_store_save(workspace_root);
    return true;
}

bool analysis_cache_load_symbols(const char* workspace_root, const char* build_args) {
    if (!workspace_root || !*workspace_root) return false;
    AnalysisCacheMeta meta = {0};
    if (!analysis_cache_load_meta(&meta, workspace_root) ||
        !analysis_cache_meta_matches(&meta, workspace_root, build_args)) {
        return false;
    }
    analysis_symbols_store_load(workspace_root);
    return true;
}

bool analysis_cache_save_tokens(const char* workspace_root) {
    if (!workspace_root || !*workspace_root) return false;
    analysis_token_store_save(workspace_root);
    return true;
}

bool analysis_cache_load_tokens(const char* workspace_root, const char* build_args) {
    if (!workspace_root || !*workspace_root) return false;
    AnalysisCacheMeta meta = {0};
    if (!analysis_cache_load_meta(&meta, workspace_root) ||
        !analysis_cache_meta_matches(&meta, workspace_root, build_args)) {
        return false;
    }
    analysis_token_store_load(workspace_root);
    return true;
}

bool analysis_cache_save_library(const char* workspace_root) {
    if (!workspace_root || !*workspace_root) return false;
    library_index_save(workspace_root);
    return true;
}

bool analysis_cache_load_library(const char* workspace_root, const char* build_args) {
    if (!workspace_root || !*workspace_root) return false;
    AnalysisCacheMeta meta = {0};
    if (!analysis_cache_load_meta(&meta, workspace_root) ||
        !analysis_cache_meta_matches(&meta, workspace_root, build_args)) {
        return false;
    }
    library_index_load(workspace_root);
    return true;
}

bool analysis_cache_save_build_flags(const BuildFlagSet* flags, const char* workspace_root) {
    if (!flags || !workspace_root || !*workspace_root) return false;
    save_build_flags(flags, workspace_root);
    return true;
}

bool analysis_cache_load_build_flags(BuildFlagSet* flags,
                                     const char* workspace_root,
                                     const char* build_args) {
    if (!flags || !workspace_root || !*workspace_root) return false;
    AnalysisCacheMeta meta = {0};
    if (!analysis_cache_load_meta(&meta, workspace_root) ||
        !analysis_cache_meta_matches(&meta, workspace_root, build_args)) {
        return false;
    }
    load_build_flags(flags, workspace_root);
    return true;
}
