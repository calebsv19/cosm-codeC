#include "core/Analysis/analysis_memory_report_store.h"

#include <json-c/json.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static AnalysisMemoryReportSnapshot* g_snapshots = NULL;
static size_t g_snapshot_count = 0;
static size_t g_snapshot_cap = 0;
static uint64_t g_stamp_counter = 0;
static pthread_mutex_t g_memory_report_mutex = PTHREAD_MUTEX_INITIALIZER;

void analysis_memory_report_store_lock(void) {
    pthread_mutex_lock(&g_memory_report_mutex);
}

void analysis_memory_report_store_unlock(void) {
    pthread_mutex_unlock(&g_memory_report_mutex);
}

static char* dup_str(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char* out = (char*)malloc(len);
    if (!out) return NULL;
    memcpy(out, s, len);
    return out;
}

static const char* json_string_value(json_object* obj) {
    return (obj && json_object_is_type(obj, json_type_string)) ? json_object_get_string(obj) : "";
}

static int json_int_value(json_object* obj) {
    return obj ? json_object_get_int(obj) : 0;
}

static uint64_t json_u64_value(json_object* obj) {
    if (!obj) return 0;
    if (json_object_is_type(obj, json_type_int)) {
        long long raw = json_object_get_int64(obj);
        return raw < 0 ? 0 : (uint64_t)raw;
    }
    return 0;
}

static void free_leak(AnalysisMemoryReportLeak* leak) {
    if (!leak) return;
    free(leak->file);
    memset(leak, 0, sizeof(*leak));
}

static void free_snapshot(AnalysisMemoryReportSnapshot* snapshot) {
    if (!snapshot) return;
    free(snapshot->path);
    free(snapshot->profile);
    free(snapshot->runtime);
    free(snapshot->trigger);
    for (size_t i = 0; i < snapshot->leak_count; ++i) {
        free_leak(&snapshot->leaks[i]);
    }
    free(snapshot->leaks);
    memset(snapshot, 0, sizeof(*snapshot));
}

void analysis_memory_report_store_clear(void) {
    analysis_memory_report_store_lock();
    for (size_t i = 0; i < g_snapshot_count; ++i) {
        free_snapshot(&g_snapshots[i]);
    }
    free(g_snapshots);
    g_snapshots = NULL;
    g_snapshot_count = 0;
    g_snapshot_cap = 0;
    g_stamp_counter = 0;
    analysis_memory_report_store_unlock();
}

static AnalysisMemoryReportSummary parse_summary(json_object* obj) {
    AnalysisMemoryReportSummary summary = {0};
    if (!obj || !json_object_is_type(obj, json_type_object)) return summary;
    json_object* value = NULL;
    if (json_object_object_get_ex(obj, "active", &value)) summary.active = json_int_value(value);
    if (json_object_object_get_ex(obj, "leaked_bytes", &value)) summary.leaked_bytes = json_u64_value(value);
    if (json_object_object_get_ex(obj, "allocs", &value)) summary.allocs = json_int_value(value);
    if (json_object_object_get_ex(obj, "frees", &value)) summary.frees = json_int_value(value);
    if (json_object_object_get_ex(obj, "double_free", &value)) summary.double_free = json_int_value(value);
    if (json_object_object_get_ex(obj, "unknown_free", &value)) summary.unknown_free = json_int_value(value);
    if (json_object_object_get_ex(obj, "tracker_failures", &value)) summary.tracker_failures = json_int_value(value);
    return summary;
}

static bool clone_leak(AnalysisMemoryReportLeak* dst, json_object* obj) {
    if (!dst || !obj || !json_object_is_type(obj, json_type_object)) return false;
    memset(dst, 0, sizeof(*dst));
    json_object* value = NULL;
    json_object_object_get_ex(obj, "size", &value);
    dst->size = json_u64_value(value);

    json_object* allocated_at = NULL;
    if (json_object_object_get_ex(obj, "allocated_at", &allocated_at) &&
        allocated_at && json_object_is_type(allocated_at, json_type_object)) {
        value = NULL;
        json_object_object_get_ex(allocated_at, "file", &value);
        dst->file = dup_str(json_string_value(value));
        value = NULL;
        json_object_object_get_ex(allocated_at, "line", &value);
        dst->line = json_int_value(value);
    } else {
        dst->file = dup_str("");
    }
    if (!dst->file) {
        free_leak(dst);
        return false;
    }
    return true;
}

static bool parse_snapshot(const char* reportPath, json_object* root, AnalysisMemoryReportSnapshot* out) {
    if (!reportPath || !root || !out || !json_object_is_type(root, json_type_object)) return false;
    memset(out, 0, sizeof(*out));
    json_object* value = NULL;
    json_object_object_get_ex(root, "profile", &value);
    const char* profile = json_string_value(value);
    if (strcmp(profile, "memory_check_report_v1") != 0) return false;

    out->path = dup_str(reportPath);
    out->profile = dup_str(profile);
    json_object_object_get_ex(root, "schema_version", &value);
    out->schema_version = json_int_value(value);
    json_object_object_get_ex(root, "runtime", &value);
    out->runtime = dup_str(json_string_value(value));
    json_object_object_get_ex(root, "trigger", &value);
    out->trigger = dup_str(json_string_value(value));
    json_object_object_get_ex(root, "summary", &value);
    out->summary = parse_summary(value);
    if (!out->path || !out->profile || !out->runtime || !out->trigger) {
        free_snapshot(out);
        return false;
    }

    json_object* leaks = NULL;
    if (json_object_object_get_ex(root, "leaks", &leaks) && leaks && json_object_is_type(leaks, json_type_array)) {
        out->leak_count = json_object_array_length(leaks);
        if (out->leak_count > 0) {
            out->leaks = (AnalysisMemoryReportLeak*)calloc(out->leak_count, sizeof(AnalysisMemoryReportLeak));
            if (!out->leaks) {
                free_snapshot(out);
                return false;
            }
            for (size_t i = 0; i < out->leak_count; ++i) {
                if (!clone_leak(&out->leaks[i], json_object_array_get_idx(leaks, i))) {
                    free_snapshot(out);
                    return false;
                }
            }
        }
    }
    return true;
}

static bool upsert_snapshot(AnalysisMemoryReportSnapshot* snapshot) {
    if (!snapshot || !snapshot->path) return false;
    analysis_memory_report_store_lock();

    size_t existing = (size_t)-1;
    for (size_t i = 0; i < g_snapshot_count; ++i) {
        if (g_snapshots[i].path && strcmp(g_snapshots[i].path, snapshot->path) == 0) {
            existing = i;
            break;
        }
    }
    if (existing != (size_t)-1) {
        free_snapshot(&g_snapshots[existing]);
        for (size_t j = existing + 1; j < g_snapshot_count; ++j) {
            g_snapshots[j - 1] = g_snapshots[j];
        }
        g_snapshot_count--;
    }

    if (g_snapshot_count >= g_snapshot_cap) {
        size_t next = g_snapshot_cap ? g_snapshot_cap * 2 : 4;
        AnalysisMemoryReportSnapshot* grown = (AnalysisMemoryReportSnapshot*)realloc(g_snapshots, next * sizeof(AnalysisMemoryReportSnapshot));
        if (!grown) {
            analysis_memory_report_store_unlock();
            return false;
        }
        memset(grown + g_snapshot_cap, 0, (next - g_snapshot_cap) * sizeof(AnalysisMemoryReportSnapshot));
        g_snapshots = grown;
        g_snapshot_cap = next;
    }

    snapshot->stamp = ++g_stamp_counter;
    for (size_t j = g_snapshot_count; j > 0; --j) {
        g_snapshots[j] = g_snapshots[j - 1];
    }
    g_snapshots[0] = *snapshot;
    memset(snapshot, 0, sizeof(*snapshot));
    g_snapshot_count++;
    analysis_memory_report_store_unlock();
    return true;
}

bool analysis_memory_report_store_load_json_text(const char* reportPath, const char* jsonText) {
    if (!reportPath || !*reportPath || !jsonText || !*jsonText) return false;
    json_object* root = json_tokener_parse(jsonText);
    if (!root) return false;
    AnalysisMemoryReportSnapshot snapshot = {0};
    bool ok = parse_snapshot(reportPath, root, &snapshot) && upsert_snapshot(&snapshot);
    free_snapshot(&snapshot);
    json_object_put(root);
    return ok;
}

bool analysis_memory_report_store_load_file(const char* reportPath) {
    if (!reportPath || !*reportPath) return false;
    FILE* f = fopen(reportPath, "r");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0 || len > (32 * 1024 * 1024)) {
        fclose(f);
        return false;
    }
    char* buf = (char*)malloc((size_t)len + 1);
    if (!buf) {
        fclose(f);
        return false;
    }
    size_t read_len = fread(buf, 1, (size_t)len, f);
    fclose(f);
    buf[read_len] = '\0';
    bool ok = analysis_memory_report_store_load_json_text(reportPath, buf);
    free(buf);
    return ok;
}

void analysis_memory_report_store_remove(const char* reportPath) {
    if (!reportPath) return;
    analysis_memory_report_store_lock();
    size_t existing = (size_t)-1;
    for (size_t i = 0; i < g_snapshot_count; ++i) {
        if (g_snapshots[i].path && strcmp(g_snapshots[i].path, reportPath) == 0) {
            existing = i;
            break;
        }
    }
    if (existing != (size_t)-1) {
        free_snapshot(&g_snapshots[existing]);
        for (size_t j = existing + 1; j < g_snapshot_count; ++j) {
            g_snapshots[j - 1] = g_snapshots[j];
        }
        g_snapshot_count--;
        g_stamp_counter++;
    }
    analysis_memory_report_store_unlock();
}

size_t analysis_memory_report_store_snapshot_count(void) {
    return g_snapshot_count;
}

const AnalysisMemoryReportSnapshot* analysis_memory_report_store_snapshot_at(size_t idx) {
    return idx < g_snapshot_count ? &g_snapshots[idx] : NULL;
}

uint64_t analysis_memory_report_store_combined_stamp(void) {
    analysis_memory_report_store_lock();
    uint64_t stamp = (uint64_t)g_snapshot_count;
    for (size_t i = 0; i < g_snapshot_count; ++i) {
        stamp ^= g_snapshots[i].stamp;
    }
    analysis_memory_report_store_unlock();
    return stamp;
}

static void ensure_cache_dir(const char* workspaceRoot) {
    if (!workspaceRoot || !*workspaceRoot) return;
    char dir[1024];
    snprintf(dir, sizeof(dir), "%s/ide_files", workspaceRoot);
    struct stat st;
    if (stat(dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
        mkdir(dir, 0755);
    }
}

static json_object* summary_to_json(const AnalysisMemoryReportSummary* summary) {
    json_object* obj = json_object_new_object();
    json_object_object_add(obj, "active", json_object_new_int(summary ? summary->active : 0));
    json_object_object_add(obj, "leaked_bytes", json_object_new_int64((long long)(summary ? summary->leaked_bytes : 0)));
    json_object_object_add(obj, "allocs", json_object_new_int(summary ? summary->allocs : 0));
    json_object_object_add(obj, "frees", json_object_new_int(summary ? summary->frees : 0));
    json_object_object_add(obj, "double_free", json_object_new_int(summary ? summary->double_free : 0));
    json_object_object_add(obj, "unknown_free", json_object_new_int(summary ? summary->unknown_free : 0));
    json_object_object_add(obj, "tracker_failures", json_object_new_int(summary ? summary->tracker_failures : 0));
    return obj;
}

static json_object* snapshot_to_json(const AnalysisMemoryReportSnapshot* snapshot) {
    json_object* obj = json_object_new_object();
    json_object_object_add(obj, "path", json_object_new_string(snapshot->path ? snapshot->path : ""));
    json_object_object_add(obj, "profile", json_object_new_string(snapshot->profile ? snapshot->profile : ""));
    json_object_object_add(obj, "schema_version", json_object_new_int(snapshot->schema_version));
    json_object_object_add(obj, "runtime", json_object_new_string(snapshot->runtime ? snapshot->runtime : ""));
    json_object_object_add(obj, "trigger", json_object_new_string(snapshot->trigger ? snapshot->trigger : ""));
    json_object_object_add(obj, "summary", summary_to_json(&snapshot->summary));
    json_object_object_add(obj, "stamp", json_object_new_int64((long long)snapshot->stamp));

    json_object* leaks = json_object_new_array();
    for (size_t i = 0; i < snapshot->leak_count; ++i) {
        const AnalysisMemoryReportLeak* leak = &snapshot->leaks[i];
        json_object* jl = json_object_new_object();
        json_object_object_add(jl, "size", json_object_new_int64((long long)leak->size));
        json_object* at = json_object_new_object();
        json_object_object_add(at, "file", json_object_new_string(leak->file ? leak->file : ""));
        json_object_object_add(at, "line", json_object_new_int(leak->line));
        json_object_object_add(jl, "allocated_at", at);
        json_object_array_add(leaks, jl);
    }
    json_object_object_add(obj, "leaks", leaks);
    return obj;
}

void analysis_memory_report_store_save(const char* workspaceRoot) {
    if (!workspaceRoot || !*workspaceRoot) return;
    ensure_cache_dir(workspaceRoot);
    char path[1024];
    snprintf(path, sizeof(path), "%s/ide_files/analysis_memory_reports.json", workspaceRoot);

    analysis_memory_report_store_lock();
    json_object* arr = json_object_new_array();
    for (size_t i = 0; i < g_snapshot_count; ++i) {
        json_object_array_add(arr, snapshot_to_json(&g_snapshots[i]));
    }
    const char* serialized = json_object_to_json_string_ext(arr, JSON_C_TO_STRING_PLAIN);
    FILE* f = fopen(path, "w");
    if (f && serialized) {
        fputs(serialized, f);
        fclose(f);
    } else if (f) {
        fclose(f);
    }
    json_object_put(arr);
    analysis_memory_report_store_unlock();
}

void analysis_memory_report_store_load(const char* workspaceRoot) {
    analysis_memory_report_store_clear();
    if (!workspaceRoot || !*workspaceRoot) return;
    char path[1024];
    snprintf(path, sizeof(path), "%s/ide_files/analysis_memory_reports.json", workspaceRoot);
    FILE* f = fopen(path, "r");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0 || len > (32 * 1024 * 1024)) {
        fclose(f);
        return;
    }
    char* buf = (char*)malloc((size_t)len + 1);
    if (!buf) {
        fclose(f);
        return;
    }
    size_t read_len = fread(buf, 1, (size_t)len, f);
    fclose(f);
    buf[read_len] = '\0';

    json_object* root = json_tokener_parse(buf);
    free(buf);
    if (!root || !json_object_is_type(root, json_type_array)) {
        if (root) json_object_put(root);
        return;
    }
    size_t count = json_object_array_length(root);
    for (size_t i = count; i > 0; --i) {
        json_object* obj = json_object_array_get_idx(root, i - 1);
        json_object* jpath = NULL;
        if (!obj || !json_object_object_get_ex(obj, "path", &jpath)) continue;
        AnalysisMemoryReportSnapshot snapshot = {0};
        if (parse_snapshot(json_string_value(jpath), obj, &snapshot)) {
            upsert_snapshot(&snapshot);
        }
        free_snapshot(&snapshot);
    }
    json_object_put(root);
}
