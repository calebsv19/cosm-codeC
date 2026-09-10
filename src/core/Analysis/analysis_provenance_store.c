#include "core/Analysis/analysis_provenance_store.h"

#include <json-c/json.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "core/Analysis/analysis_artifact_io.h"

#define ANALYSIS_PROVENANCE_ARTIFACT "analysis_provenance.json"

static AnalysisProvenanceRecord* g_records = NULL;
static size_t g_record_count = 0;
static size_t g_record_capacity = 0;
static uint64_t g_generation_counter = 0;
static uint64_t g_stamp_counter = 0;
static pthread_mutex_t g_provenance_mutex = PTHREAD_MUTEX_INITIALIZER;

static void copy_text(char* dst, size_t dst_cap, const char* src) {
    if (!dst || dst_cap == 0) return;
    snprintf(dst, dst_cap, "%s", src ? src : "");
    dst[dst_cap - 1] = '\0';
}

static uint64_t source_hash_fnv1a64(const char* source, size_t length) {
    const uint64_t basis = 1469598103934665603ULL;
    const uint64_t prime = 1099511628211ULL;
    uint64_t hash = basis;
    if (!source || length == 0) return hash;
    for (size_t i = 0; i < length; ++i) {
        hash ^= (unsigned char)source[i];
        hash *= prime;
    }
    return hash;
}

static uint64_t unix_time_ms(void) {
    time_t now = time(NULL);
    return now < 0 ? 0u : (uint64_t)now * 1000u;
}

static AnalysisAuthorityState derive_state(const AnalysisProvenanceRecord* record) {
    if (!record) return ANALYSIS_AUTHORITY_DEGRADED;
    if (record->fatal) return ANALYSIS_AUTHORITY_FATAL;
    if (record->degraded) return ANALYSIS_AUTHORITY_DEGRADED;
    if (record->partial) return ANALYSIS_AUTHORITY_PARTIAL;
    if (!record->source_match) return ANALYSIS_AUTHORITY_STALE;
    return ANALYSIS_AUTHORITY_CURRENT;
}

static size_t find_record_locked(const char* file_path) {
    if (!file_path) return (size_t)-1;
    for (size_t i = 0; i < g_record_count; ++i) {
        if (strcmp(g_records[i].file_path, file_path) == 0) return i;
    }
    return (size_t)-1;
}

static bool ensure_capacity_locked(void) {
    if (g_record_count < g_record_capacity) return true;
    size_t next = g_record_capacity ? g_record_capacity * 2u : 16u;
    AnalysisProvenanceRecord* grown =
        (AnalysisProvenanceRecord*)realloc(g_records, next * sizeof(*g_records));
    if (!grown) return false;
    memset(grown + g_record_capacity, 0,
           (next - g_record_capacity) * sizeof(*g_records));
    g_records = grown;
    g_record_capacity = next;
    return true;
}

void analysis_provenance_store_clear(void) {
    pthread_mutex_lock(&g_provenance_mutex);
    free(g_records);
    g_records = NULL;
    g_record_count = 0;
    g_record_capacity = 0;
    g_generation_counter = 0;
    g_stamp_counter = 0;
    pthread_mutex_unlock(&g_provenance_mutex);
}

void analysis_provenance_store_upsert(const char* file_path,
                                      const char* source,
                                      size_t source_length,
                                      const FisicsAnalysisContract* contract,
                                      uint64_t effective_capabilities,
                                      bool degraded,
                                      const char* degraded_reason) {
    if (!file_path || !file_path[0] || !contract) return;

    AnalysisProvenanceRecord next = {0};
    copy_text(next.file_path, sizeof(next.file_path), file_path);
    copy_text(next.contract_id, sizeof(next.contract_id), contract->contract_id);
    next.contract_major = contract->contract_major;
    next.contract_minor = contract->contract_minor;
    next.contract_patch = contract->contract_patch;
    copy_text(next.producer_name, sizeof(next.producer_name), contract->producer_name);
    copy_text(next.producer_version, sizeof(next.producer_version), contract->producer_version);
    next.mode = contract->mode;
    next.partial = contract->partial;
    next.fatal = contract->fatal;
    next.source_hash = contract->source_hash;
    next.source_length = contract->source_length;
    next.advertised_capabilities = contract->capabilities;
    next.effective_capabilities = effective_capabilities;
    next.capability_flags_present = contract->contract_major == 1 &&
                                    contract->contract_minor >= 4;
    next.source_match = source &&
                        contract->source_length == (uint64_t)source_length &&
                        contract->source_hash == source_hash_fnv1a64(source, source_length);
    next.loaded_from_cache = false;
    next.degraded = degraded;
    copy_text(next.degraded_reason, sizeof(next.degraded_reason), degraded_reason);
    next.observed_at_unix_ms = unix_time_ms();

    pthread_mutex_lock(&g_provenance_mutex);
    size_t index = find_record_locked(file_path);
    if (index == (size_t)-1) {
        if (!ensure_capacity_locked()) {
            pthread_mutex_unlock(&g_provenance_mutex);
            return;
        }
        index = g_record_count++;
    }
    next.generation = ++g_generation_counter;
    next.stamp = ++g_stamp_counter;
    next.state = derive_state(&next);
    g_records[index] = next;
    pthread_mutex_unlock(&g_provenance_mutex);
}

void analysis_provenance_store_remove(const char* file_path) {
    if (!file_path || !file_path[0]) return;
    pthread_mutex_lock(&g_provenance_mutex);
    size_t index = find_record_locked(file_path);
    if (index != (size_t)-1) {
        for (size_t i = index + 1; i < g_record_count; ++i) {
            g_records[i - 1] = g_records[i];
        }
        g_record_count--;
        g_stamp_counter++;
    }
    pthread_mutex_unlock(&g_provenance_mutex);
}

size_t analysis_provenance_store_file_count(void) {
    pthread_mutex_lock(&g_provenance_mutex);
    size_t count = g_record_count;
    pthread_mutex_unlock(&g_provenance_mutex);
    return count;
}

bool analysis_provenance_store_copy_at(size_t index, AnalysisProvenanceRecord* out) {
    if (!out) return false;
    pthread_mutex_lock(&g_provenance_mutex);
    bool found = index < g_record_count;
    if (found) *out = g_records[index];
    pthread_mutex_unlock(&g_provenance_mutex);
    return found;
}

bool analysis_provenance_store_copy_for_file(const char* file_path,
                                             AnalysisProvenanceRecord* out) {
    if (!file_path || !out) return false;
    pthread_mutex_lock(&g_provenance_mutex);
    size_t index = find_record_locked(file_path);
    bool found = index != (size_t)-1;
    if (found) *out = g_records[index];
    pthread_mutex_unlock(&g_provenance_mutex);
    return found;
}

static void summarize_locked(AnalysisProvenanceSummary* out) {
    if (!out) return;
    memset(out, 0, sizeof(*out));
    out->worst_state = ANALYSIS_AUTHORITY_CURRENT;
    out->total_count = g_record_count;
    for (size_t i = 0; i < g_record_count; ++i) {
        AnalysisAuthorityState state = g_records[i].state;
        if (state > out->worst_state) out->worst_state = state;
        switch (state) {
            case ANALYSIS_AUTHORITY_CURRENT: out->current_count++; break;
            case ANALYSIS_AUTHORITY_STALE: out->stale_count++; break;
            case ANALYSIS_AUTHORITY_PARTIAL: out->partial_count++; break;
            case ANALYSIS_AUTHORITY_DEGRADED: out->degraded_count++; break;
            case ANALYSIS_AUTHORITY_FATAL: out->fatal_count++; break;
        }
    }
}

static uint64_t combined_stamp_locked(void) {
    uint64_t stamp = (uint64_t)g_record_count;
    for (size_t i = 0; i < g_record_count; ++i) stamp ^= g_records[i].stamp;
    return stamp;
}

bool analysis_provenance_store_copy_snapshot(AnalysisProvenanceRecord** out_records,
                                             size_t* out_count,
                                             AnalysisProvenanceSummary* out_summary,
                                             uint64_t* out_stamp) {
    if (!out_records || !out_count) return false;
    *out_records = NULL;
    *out_count = 0;
    pthread_mutex_lock(&g_provenance_mutex);
    AnalysisProvenanceRecord* records = NULL;
    if (g_record_count > 0) {
        records = malloc(g_record_count * sizeof(*records));
        if (!records) {
            pthread_mutex_unlock(&g_provenance_mutex);
            return false;
        }
        memcpy(records, g_records, g_record_count * sizeof(*records));
    }
    *out_records = records;
    *out_count = g_record_count;
    if (out_summary) summarize_locked(out_summary);
    if (out_stamp) *out_stamp = combined_stamp_locked();
    pthread_mutex_unlock(&g_provenance_mutex);
    return true;
}

void analysis_provenance_store_free_snapshot(AnalysisProvenanceRecord* records) {
    free(records);
}

void analysis_provenance_store_summary(AnalysisProvenanceSummary* out) {
    if (!out) return;
    pthread_mutex_lock(&g_provenance_mutex);
    summarize_locked(out);
    pthread_mutex_unlock(&g_provenance_mutex);
}

uint64_t analysis_provenance_store_combined_stamp(void) {
    pthread_mutex_lock(&g_provenance_mutex);
    uint64_t stamp = combined_stamp_locked();
    pthread_mutex_unlock(&g_provenance_mutex);
    return stamp;
}

const char* analysis_authority_state_name(AnalysisAuthorityState state) {
    switch (state) {
        case ANALYSIS_AUTHORITY_CURRENT: return "current";
        case ANALYSIS_AUTHORITY_STALE: return "stale";
        case ANALYSIS_AUTHORITY_PARTIAL: return "partial";
        case ANALYSIS_AUTHORITY_DEGRADED: return "degraded";
        case ANALYSIS_AUTHORITY_FATAL: return "fatal";
        default: return "degraded";
    }
}

const char* analysis_provenance_state_reason(const AnalysisProvenanceRecord* record) {
    if (!record) return "missing provenance record";
    switch (record->state) {
        case ANALYSIS_AUTHORITY_FATAL:
            return "frontend marked result fatal";
        case ANALYSIS_AUTHORITY_DEGRADED:
            return record->degraded_reason[0] ? record->degraded_reason
                                              : "contract compatibility degraded";
        case ANALYSIS_AUTHORITY_PARTIAL:
            return "frontend marked result partial";
        case ANALYSIS_AUTHORITY_STALE:
            return record->loaded_from_cache
                ? "cached result awaiting source verification"
                : "frontend source identity does not match analyzed buffer";
        case ANALYSIS_AUTHORITY_CURRENT:
        default:
            return "frontend source identity matches analyzed buffer";
    }
}

static json_object* json_u64_hex(uint64_t value) {
    char text[19];
    snprintf(text, sizeof(text), "0x%016llx", (unsigned long long)value);
    return json_object_new_string(text);
}

static uint64_t parse_u64(json_object* value) {
    if (!value) return 0;
    if (json_object_is_type(value, json_type_string)) {
        const char* text = json_object_get_string(value);
        char* end = NULL;
        unsigned long long parsed = strtoull(text ? text : "", &end, 0);
        return end && *end == '\0' ? (uint64_t)parsed : 0;
    }
    if (json_object_is_type(value, json_type_int)) {
        long long parsed = json_object_get_int64(value);
        return parsed < 0 ? 0u : (uint64_t)parsed;
    }
    return 0;
}

void analysis_provenance_store_save(const char* workspace_root) {
    if (!workspace_root || !workspace_root[0]) return;
    json_object* root = json_object_new_object();
    json_object* records = json_object_new_array();
    json_object_object_add(root, "schema", json_object_new_string("ide.analysis_provenance"));
    json_object_object_add(root, "version", json_object_new_int(1));

    pthread_mutex_lock(&g_provenance_mutex);
    for (size_t i = 0; i < g_record_count; ++i) {
        const AnalysisProvenanceRecord* record = &g_records[i];
        json_object* obj = json_object_new_object();
        json_object_object_add(obj, "file", json_object_new_string(record->file_path));
        json_object_object_add(obj, "contract_id", json_object_new_string(record->contract_id));
        json_object_object_add(obj, "contract_major", json_object_new_int(record->contract_major));
        json_object_object_add(obj, "contract_minor", json_object_new_int(record->contract_minor));
        json_object_object_add(obj, "contract_patch", json_object_new_int(record->contract_patch));
        json_object_object_add(obj, "producer_name", json_object_new_string(record->producer_name));
        json_object_object_add(obj, "producer_version", json_object_new_string(record->producer_version));
        json_object_object_add(obj, "mode", json_object_new_int((int)record->mode));
        json_object_object_add(obj, "partial", json_object_new_boolean(record->partial));
        json_object_object_add(obj, "fatal", json_object_new_boolean(record->fatal));
        json_object_object_add(obj, "source_hash", json_u64_hex(record->source_hash));
        json_object_object_add(obj, "source_length", json_u64_hex(record->source_length));
        json_object_object_add(obj, "advertised_capabilities", json_u64_hex(record->advertised_capabilities));
        json_object_object_add(obj, "effective_capabilities", json_u64_hex(record->effective_capabilities));
        json_object_object_add(obj, "capability_flags_present", json_object_new_boolean(record->capability_flags_present));
        json_object_object_add(obj, "degraded", json_object_new_boolean(record->degraded));
        json_object_object_add(obj, "degraded_reason", json_object_new_string(record->degraded_reason));
        json_object_object_add(obj, "generation", json_u64_hex(record->generation));
        json_object_object_add(obj, "observed_at_unix_ms", json_u64_hex(record->observed_at_unix_ms));
        json_object_array_add(records, obj);
    }
    pthread_mutex_unlock(&g_provenance_mutex);
    json_object_object_add(root, "records", records);

    const char* text = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
    if (text) {
        (void)analysis_artifact_io_write_text(workspace_root,
                                              ANALYSIS_PROVENANCE_ARTIFACT,
                                              text);
    }
    json_object_put(root);
}

void analysis_provenance_store_load(const char* workspace_root) {
    analysis_provenance_store_clear();
    if (!workspace_root || !workspace_root[0]) return;
    char* text = analysis_artifact_io_read_text(workspace_root,
                                                ANALYSIS_PROVENANCE_ARTIFACT,
                                                ANALYSIS_ARTIFACT_IO_DEFAULT_MAX_BYTES,
                                                NULL);
    if (!text) return;
    json_object* root = json_tokener_parse(text);
    free(text);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        return;
    }
    json_object *schema = NULL, *version = NULL, *records = NULL;
    if (!json_object_object_get_ex(root, "schema", &schema) ||
        strcmp(json_object_get_string(schema), "ide.analysis_provenance") != 0 ||
        !json_object_object_get_ex(root, "version", &version) ||
        json_object_get_int(version) != 1 ||
        !json_object_object_get_ex(root, "records", &records) ||
        !json_object_is_type(records, json_type_array)) {
        json_object_put(root);
        return;
    }

    pthread_mutex_lock(&g_provenance_mutex);
    size_t count = json_object_array_length(records);
    for (size_t i = 0; i < count; ++i) {
        json_object* obj = json_object_array_get_idx(records, i);
        json_object* value = NULL;
        if (!obj || !json_object_object_get_ex(obj, "file", &value)) continue;
        const char* file = json_object_get_string(value);
        if (!file || !file[0] || !ensure_capacity_locked()) continue;
        AnalysisProvenanceRecord record = {0};
        copy_text(record.file_path, sizeof(record.file_path), file);
#define LOAD_TEXT(name, field) do { \
            value = NULL; \
            if (json_object_object_get_ex(obj, name, &value)) \
                copy_text(record.field, sizeof(record.field), json_object_get_string(value)); \
        } while (0)
#define LOAD_INT(name, field) do { \
            value = NULL; \
            if (json_object_object_get_ex(obj, name, &value)) record.field = json_object_get_int(value); \
        } while (0)
#define LOAD_BOOL(name, field) do { \
            value = NULL; \
            if (json_object_object_get_ex(obj, name, &value)) record.field = json_object_get_boolean(value); \
        } while (0)
#define LOAD_U64(name, field) do { \
            value = NULL; \
            if (json_object_object_get_ex(obj, name, &value)) record.field = parse_u64(value); \
        } while (0)
        LOAD_TEXT("contract_id", contract_id);
        LOAD_INT("contract_major", contract_major);
        LOAD_INT("contract_minor", contract_minor);
        LOAD_INT("contract_patch", contract_patch);
        LOAD_TEXT("producer_name", producer_name);
        LOAD_TEXT("producer_version", producer_version);
        LOAD_INT("mode", mode);
        LOAD_BOOL("partial", partial);
        LOAD_BOOL("fatal", fatal);
        LOAD_U64("source_hash", source_hash);
        LOAD_U64("source_length", source_length);
        LOAD_U64("advertised_capabilities", advertised_capabilities);
        LOAD_U64("effective_capabilities", effective_capabilities);
        LOAD_BOOL("capability_flags_present", capability_flags_present);
        LOAD_BOOL("degraded", degraded);
        LOAD_TEXT("degraded_reason", degraded_reason);
        LOAD_U64("generation", generation);
        LOAD_U64("observed_at_unix_ms", observed_at_unix_ms);
#undef LOAD_TEXT
#undef LOAD_INT
#undef LOAD_BOOL
#undef LOAD_U64
        record.loaded_from_cache = true;
        record.source_match = false;
        record.state = derive_state(&record);
        record.stamp = ++g_stamp_counter;
        if (record.generation > g_generation_counter) g_generation_counter = record.generation;
        g_records[g_record_count++] = record;
    }
    pthread_mutex_unlock(&g_provenance_mutex);
    json_object_put(root);
}
