#include "core/Analysis/analysis_units_store.h"

#include <json-c/json.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "core/LoopKernel/mainthread_context.h"

static AnalysisFileUnits* g_files = NULL;
static size_t g_file_count = 0;
static size_t g_file_cap = 0;
static uint64_t g_stamp_counter = 0;
static pthread_mutex_t g_units_mutex = PTHREAD_MUTEX_INITIALIZER;

void analysis_units_store_lock(void) {
    pthread_mutex_lock(&g_units_mutex);
}

void analysis_units_store_unlock(void) {
    pthread_mutex_unlock(&g_units_mutex);
}

static char* dup_str(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char* out = (char*)malloc(len);
    if (!out) return NULL;
    memcpy(out, s, len);
    return out;
}

static void free_attachment(AnalysisUnitsAttachment* att) {
    if (!att) return;
    free(att->symbol_name);
    free(att->dim_text);
    free(att->unit_source_text);
    free(att->unit_name);
    free(att->unit_symbol);
    free(att->unit_family);
    memset(att, 0, sizeof(*att));
}

static void free_entry(AnalysisFileUnits* f) {
    if (!f) return;
    free(f->path);
    for (size_t i = 0; i < f->count; ++i) {
        free_attachment(&f->attachments[i]);
    }
    free(f->attachments);
    memset(f, 0, sizeof(*f));
}

void analysis_units_store_clear(void) {
    mainthread_context_assert_owner("analysis_units_store.clear");
    analysis_units_store_lock();
    for (size_t i = 0; i < g_file_count; ++i) {
        free_entry(&g_files[i]);
    }
    free(g_files);
    g_files = NULL;
    g_file_count = 0;
    g_file_cap = 0;
    g_stamp_counter = 0;
    analysis_units_store_unlock();
}

static bool clone_attachment(AnalysisUnitsAttachment* dst,
                             const FisicsUnitsAttachment* src,
                             bool concreteUnitFieldsEnabled) {
    if (!dst || !src) return false;
    memset(dst, 0, sizeof(*dst));
    dst->symbol_stable_id = src->symbol_stable_id;
    dst->symbol_name = dup_str(src->symbol_name);
    dst->dim_text = dup_str(src->dim_text);
    for (size_t i = 0; i < FISICS_UNITS_DIM_SLOTS; ++i) {
        dst->dim[i] = src->dim[i];
    }
    dst->resolved = src->resolved;

    if (concreteUnitFieldsEnabled) {
        dst->unit_source_text = dup_str(src->unit_source_text);
        dst->unit_name = dup_str(src->unit_name);
        dst->unit_symbol = dup_str(src->unit_symbol);
        dst->unit_family = dup_str(src->unit_family);
        dst->unit_resolved = src->unit_resolved;
        dst->has_concrete_unit = (src->unit_source_text && src->unit_source_text[0]) ||
                                 (src->unit_name && src->unit_name[0]) ||
                                 (src->unit_symbol && src->unit_symbol[0]) ||
                                 (src->unit_family && src->unit_family[0]) ||
                                 src->unit_resolved;
    }

    if ((src->symbol_name && !dst->symbol_name) ||
        (src->dim_text && !dst->dim_text) ||
        (concreteUnitFieldsEnabled && src->unit_source_text && !dst->unit_source_text) ||
        (concreteUnitFieldsEnabled && src->unit_name && !dst->unit_name) ||
        (concreteUnitFieldsEnabled && src->unit_symbol && !dst->unit_symbol) ||
        (concreteUnitFieldsEnabled && src->unit_family && !dst->unit_family)) {
        free_attachment(dst);
        return false;
    }
    return true;
}

void analysis_units_store_upsert(const char* filePath,
                                 const FisicsUnitsAttachment* attachments,
                                 size_t attachmentCount,
                                 bool concreteUnitFieldsEnabled) {
    if (!filePath) return;
    mainthread_context_assert_owner("analysis_units_store.upsert");
    analysis_units_store_lock();

    size_t existing = (size_t)-1;
    for (size_t i = 0; i < g_file_count; ++i) {
        if (g_files[i].path && strcmp(g_files[i].path, filePath) == 0) {
            existing = i;
            break;
        }
    }
    if (existing != (size_t)-1) {
        free_entry(&g_files[existing]);
        for (size_t j = existing + 1; j < g_file_count; ++j) {
            g_files[j - 1] = g_files[j];
        }
        g_file_count--;
    }

    if (g_file_count >= g_file_cap) {
        size_t next = g_file_cap ? g_file_cap * 2 : 8;
        AnalysisFileUnits* grown = (AnalysisFileUnits*)realloc(g_files, next * sizeof(AnalysisFileUnits));
        if (!grown) {
            analysis_units_store_unlock();
            return;
        }
        memset(grown + g_file_cap, 0, (next - g_file_cap) * sizeof(AnalysisFileUnits));
        g_files = grown;
        g_file_cap = next;
    }

    AnalysisFileUnits entry = {0};
    entry.path = dup_str(filePath);
    entry.count = attachmentCount;
    entry.stamp = ++g_stamp_counter;
    if (!entry.path) {
        analysis_units_store_unlock();
        return;
    }

    if (attachmentCount > 0 && attachments) {
        entry.attachments = (AnalysisUnitsAttachment*)calloc(attachmentCount, sizeof(AnalysisUnitsAttachment));
        if (!entry.attachments) {
            free_entry(&entry);
            analysis_units_store_unlock();
            return;
        }
        for (size_t i = 0; i < attachmentCount; ++i) {
            if (!clone_attachment(&entry.attachments[i], &attachments[i], concreteUnitFieldsEnabled)) {
                entry.count = i + 1;
                free_entry(&entry);
                analysis_units_store_unlock();
                return;
            }
        }
    }

    for (size_t j = g_file_count; j > 0; --j) {
        g_files[j] = g_files[j - 1];
    }
    g_files[0] = entry;
    g_file_count++;
    analysis_units_store_unlock();
}

void analysis_units_store_remove(const char* filePath) {
    if (!filePath) return;
    mainthread_context_assert_owner("analysis_units_store.remove");
    analysis_units_store_lock();
    size_t existing = (size_t)-1;
    for (size_t i = 0; i < g_file_count; ++i) {
        if (g_files[i].path && strcmp(g_files[i].path, filePath) == 0) {
            existing = i;
            break;
        }
    }
    if (existing != (size_t)-1) {
        free_entry(&g_files[existing]);
        for (size_t j = existing + 1; j < g_file_count; ++j) {
            g_files[j - 1] = g_files[j];
        }
        g_file_count--;
        g_stamp_counter++;
    }
    analysis_units_store_unlock();
}

size_t analysis_units_store_file_count(void) {
    return g_file_count;
}

const AnalysisFileUnits* analysis_units_store_file_at(size_t idx) {
    return idx < g_file_count ? &g_files[idx] : NULL;
}

const AnalysisUnitsAttachment* analysis_units_store_find_by_symbol_id(uint64_t symbolStableId) {
    if (symbolStableId == 0) return NULL;
    for (size_t fi = 0; fi < g_file_count; ++fi) {
        const AnalysisFileUnits* file = &g_files[fi];
        for (size_t i = 0; i < file->count; ++i) {
            if (file->attachments[i].symbol_stable_id == symbolStableId) {
                return &file->attachments[i];
            }
        }
    }
    return NULL;
}

uint64_t analysis_units_store_combined_stamp(void) {
    analysis_units_store_lock();
    uint64_t stamp = (uint64_t)g_file_count;
    for (size_t i = 0; i < g_file_count; ++i) {
        stamp ^= g_files[i].stamp;
    }
    analysis_units_store_unlock();
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

static json_object* json_string_or_empty(const char* s) {
    return json_object_new_string(s ? s : "");
}

static void format_u64_hex(uint64_t value, char out[19]) {
    if (!out) return;
    snprintf(out, 19, "0x%016llx", (unsigned long long)value);
}

static uint64_t parse_json_u64(json_object* value) {
    if (!value) return 0;
    if (json_object_is_type(value, json_type_int)) {
        long long raw = json_object_get_int64(value);
        return raw < 0 ? 0 : (uint64_t)raw;
    }
    if (json_object_is_type(value, json_type_string)) {
        const char* text = json_object_get_string(value);
        if (!text || !*text) return 0;
        char* end = NULL;
        unsigned long long parsed = strtoull(text, &end, 0);
        if (!end || *end != '\0') return 0;
        return (uint64_t)parsed;
    }
    return 0;
}

void analysis_units_store_save(const char* workspaceRoot) {
    if (!workspaceRoot || !*workspaceRoot) return;
    ensure_cache_dir(workspaceRoot);
    char path[1024];
    snprintf(path, sizeof(path), "%s/ide_files/analysis_units_attachments.json", workspaceRoot);

    analysis_units_store_lock();
    json_object* arr = json_object_new_array();
    for (size_t fi = 0; fi < g_file_count; ++fi) {
        const AnalysisFileUnits* file = &g_files[fi];
        json_object* obj = json_object_new_object();
        json_object_object_add(obj, "path", json_string_or_empty(file->path));
        json_object_object_add(obj, "stamp", json_object_new_int64((long long)file->stamp));
        json_object* atts = json_object_new_array();
        for (size_t i = 0; i < file->count; ++i) {
            const AnalysisUnitsAttachment* att = &file->attachments[i];
            json_object* ja = json_object_new_object();
            char stable_id_hex[19];
            format_u64_hex(att->symbol_stable_id, stable_id_hex);
            json_object_object_add(ja, "symbol_stable_id", json_object_new_string(stable_id_hex));
            json_object_object_add(ja, "symbol_name", json_string_or_empty(att->symbol_name));
            json_object_object_add(ja, "dim_text", json_string_or_empty(att->dim_text));
            json_object* dim = json_object_new_array();
            for (size_t d = 0; d < FISICS_UNITS_DIM_SLOTS; ++d) {
                json_object_array_add(dim, json_object_new_int((int)att->dim[d]));
            }
            json_object_object_add(ja, "dim", dim);
            json_object_object_add(ja, "resolved", json_object_new_boolean(att->resolved));
            json_object_object_add(ja, "has_concrete_unit", json_object_new_boolean(att->has_concrete_unit));
            if (att->has_concrete_unit) {
                json_object_object_add(ja, "unit_source_text", json_string_or_empty(att->unit_source_text));
                json_object_object_add(ja, "unit_name", json_string_or_empty(att->unit_name));
                json_object_object_add(ja, "unit_symbol", json_string_or_empty(att->unit_symbol));
                json_object_object_add(ja, "unit_family", json_string_or_empty(att->unit_family));
                json_object_object_add(ja, "unit_resolved", json_object_new_boolean(att->unit_resolved));
            }
            json_object_array_add(atts, ja);
        }
        json_object_object_add(obj, "attachments", atts);
        json_object_array_add(arr, obj);
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
    analysis_units_store_unlock();
}

void analysis_units_store_load(const char* workspaceRoot) {
    mainthread_context_assert_owner("analysis_units_store.load");
    analysis_units_store_clear();
    if (!workspaceRoot || !*workspaceRoot) return;
    char path[1024];
    snprintf(path, sizeof(path), "%s/ide_files/analysis_units_attachments.json", workspaceRoot);
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
    size_t file_count = json_object_array_length(root);
    for (size_t fi = 0; fi < file_count; ++fi) {
        json_object* obj = json_object_array_get_idx(root, fi);
        if (!obj) continue;
        json_object* jpath = NULL;
        json_object* jstamp = NULL;
        json_object* jatts = NULL;
        if (!json_object_object_get_ex(obj, "path", &jpath) ||
            !json_object_object_get_ex(obj, "attachments", &jatts) ||
            !json_object_is_type(jatts, json_type_array)) {
            continue;
        }
        const char* path_str = json_object_get_string(jpath);
        size_t count = json_object_array_length(jatts);
        FisicsUnitsAttachment* tmp = (FisicsUnitsAttachment*)calloc(count, sizeof(FisicsUnitsAttachment));
        if (!tmp) continue;
        bool* concrete = (bool*)calloc(count ? count : 1, sizeof(bool));
        if (!concrete) {
            free(tmp);
            continue;
        }
        for (size_t i = 0; i < count; ++i) {
            json_object* ja = json_object_array_get_idx(jatts, i);
            if (!ja) continue;
            json_object* jid=NULL,* jname=NULL,* jdimText=NULL,* jdim=NULL,* jresolved=NULL,* jhasConcrete=NULL;
            json_object* jus=NULL,* jun=NULL,* jusym=NULL,* juf=NULL,* jur=NULL;
            json_object_object_get_ex(ja, "symbol_stable_id", &jid);
            json_object_object_get_ex(ja, "symbol_name", &jname);
            json_object_object_get_ex(ja, "dim_text", &jdimText);
            json_object_object_get_ex(ja, "dim", &jdim);
            json_object_object_get_ex(ja, "resolved", &jresolved);
            json_object_object_get_ex(ja, "has_concrete_unit", &jhasConcrete);
            json_object_object_get_ex(ja, "unit_source_text", &jus);
            json_object_object_get_ex(ja, "unit_name", &jun);
            json_object_object_get_ex(ja, "unit_symbol", &jusym);
            json_object_object_get_ex(ja, "unit_family", &juf);
            json_object_object_get_ex(ja, "unit_resolved", &jur);
            tmp[i].symbol_stable_id = parse_json_u64(jid);
            tmp[i].symbol_name = dup_str(jname ? json_object_get_string(jname) : NULL);
            tmp[i].dim_text = dup_str(jdimText ? json_object_get_string(jdimText) : NULL);
            if (jdim && json_object_is_type(jdim, json_type_array)) {
                size_t len_dim = json_object_array_length(jdim);
                size_t limit = len_dim < FISICS_UNITS_DIM_SLOTS ? len_dim : FISICS_UNITS_DIM_SLOTS;
                for (size_t d = 0; d < limit; ++d) {
                    json_object* slot = json_object_array_get_idx(jdim, d);
                    tmp[i].dim[d] = slot ? (int8_t)json_object_get_int(slot) : 0;
                }
            }
            tmp[i].resolved = jresolved ? json_object_get_boolean(jresolved) : false;
            concrete[i] = jhasConcrete ? json_object_get_boolean(jhasConcrete) : false;
            if (concrete[i]) {
                tmp[i].unit_source_text = dup_str(jus ? json_object_get_string(jus) : NULL);
                tmp[i].unit_name = dup_str(jun ? json_object_get_string(jun) : NULL);
                tmp[i].unit_symbol = dup_str(jusym ? json_object_get_string(jusym) : NULL);
                tmp[i].unit_family = dup_str(juf ? json_object_get_string(juf) : NULL);
                tmp[i].unit_resolved = jur ? json_object_get_boolean(jur) : false;
            }
        }
        bool any_concrete = false;
        for (size_t i = 0; i < count; ++i) {
            any_concrete = any_concrete || concrete[i];
        }
        analysis_units_store_upsert(path_str, tmp, count, any_concrete);
        for (size_t i = 0; i < count; ++i) {
            free((char*)tmp[i].symbol_name);
            free((char*)tmp[i].dim_text);
            free((char*)tmp[i].unit_source_text);
            free((char*)tmp[i].unit_name);
            free((char*)tmp[i].unit_symbol);
            free((char*)tmp[i].unit_family);
        }
        free(concrete);
        free(tmp);
        if (json_object_object_get_ex(obj, "stamp", &jstamp)) {
            long long s = json_object_get_int64(jstamp);
            if (s > 0 && (uint64_t)s > g_stamp_counter) g_stamp_counter = (uint64_t)s;
        }
    }
    json_object_put(root);
}
