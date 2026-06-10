#include "core/Analysis/analysis_build_graph_store.h"

#include <json-c/json.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static AnalysisBuildGraphSnapshot* g_snapshots = NULL;
static size_t g_snapshot_count = 0;
static size_t g_snapshot_cap = 0;
static uint64_t g_stamp_counter = 0;
static pthread_mutex_t g_build_graph_mutex = PTHREAD_MUTEX_INITIALIZER;

void analysis_build_graph_store_lock(void) {
    pthread_mutex_lock(&g_build_graph_mutex);
}

void analysis_build_graph_store_unlock(void) {
    pthread_mutex_unlock(&g_build_graph_mutex);
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

static bool json_bool_value(json_object* obj) {
    return obj ? json_object_get_boolean(obj) : false;
}

static int json_int_value(json_object* obj) {
    return obj ? json_object_get_int(obj) : 0;
}

static void free_translation_unit(AnalysisBuildGraphTranslationUnit* tu) {
    if (!tu) return;
    free(tu->id);
    free(tu->source);
    free(tu->object);
    free(tu->status);
    memset(tu, 0, sizeof(*tu));
}

static void free_action(AnalysisBuildGraphAction* action) {
    if (!action) return;
    free(action->id);
    free(action->kind);
    free(action->status);
    free(action->source);
    free(action->object);
    free(action->output);
    memset(action, 0, sizeof(*action));
}

static void free_snapshot(AnalysisBuildGraphSnapshot* snapshot) {
    if (!snapshot) return;
    free(snapshot->path);
    free(snapshot->schema);
    free(snapshot->mode);
    free(snapshot->project_root);
    for (size_t i = 0; i < snapshot->translation_unit_count; ++i) {
        free_translation_unit(&snapshot->translation_units[i]);
    }
    free(snapshot->translation_units);
    for (size_t i = 0; i < snapshot->action_count; ++i) {
        free_action(&snapshot->actions[i]);
    }
    free(snapshot->actions);
    memset(snapshot, 0, sizeof(*snapshot));
}

void analysis_build_graph_store_clear(void) {
    analysis_build_graph_store_lock();
    for (size_t i = 0; i < g_snapshot_count; ++i) {
        free_snapshot(&g_snapshots[i]);
    }
    free(g_snapshots);
    g_snapshots = NULL;
    g_snapshot_count = 0;
    g_snapshot_cap = 0;
    g_stamp_counter = 0;
    analysis_build_graph_store_unlock();
}

static AnalysisBuildGraphDiagnosticSummary parse_summary(json_object* obj) {
    AnalysisBuildGraphDiagnosticSummary summary = {0};
    if (!obj || !json_object_is_type(obj, json_type_object)) return summary;
    json_object* value = NULL;
    if (json_object_object_get_ex(obj, "available", &value)) summary.available = json_bool_value(value);
    if (json_object_object_get_ex(obj, "total", &value)) summary.total = json_int_value(value);
    if (json_object_object_get_ex(obj, "errors", &value)) summary.errors = json_int_value(value);
    if (json_object_object_get_ex(obj, "warnings", &value)) summary.warnings = json_int_value(value);
    if (json_object_object_get_ex(obj, "notes", &value)) summary.notes = json_int_value(value);
    if (json_object_object_get_ex(obj, "partial", &value)) summary.partial = json_bool_value(value);
    if (json_object_object_get_ex(obj, "fatal", &value)) summary.fatal = json_bool_value(value);
    return summary;
}

static bool clone_translation_unit(AnalysisBuildGraphTranslationUnit* dst, json_object* obj) {
    if (!dst || !obj || !json_object_is_type(obj, json_type_object)) return false;
    memset(dst, 0, sizeof(*dst));
    json_object* value = NULL;
    json_object_object_get_ex(obj, "id", &value);
    dst->id = dup_str(json_string_value(value));
    json_object_object_get_ex(obj, "source", &value);
    dst->source = dup_str(json_string_value(value));
    json_object_object_get_ex(obj, "object", &value);
    dst->object = dup_str(json_string_value(value));
    json_object_object_get_ex(obj, "status", &value);
    dst->status = dup_str(json_string_value(value));
    json_object_object_get_ex(obj, "diagnostic_summary", &value);
    dst->diagnostic_summary = parse_summary(value);
    if (!dst->id || !dst->source || !dst->object || !dst->status) {
        free_translation_unit(dst);
        return false;
    }
    return true;
}

static bool clone_action(AnalysisBuildGraphAction* dst, json_object* obj) {
    if (!dst || !obj || !json_object_is_type(obj, json_type_object)) return false;
    memset(dst, 0, sizeof(*dst));
    json_object* value = NULL;
    json_object_object_get_ex(obj, "id", &value);
    dst->id = dup_str(json_string_value(value));
    json_object_object_get_ex(obj, "kind", &value);
    dst->kind = dup_str(json_string_value(value));
    json_object_object_get_ex(obj, "status", &value);
    dst->status = dup_str(json_string_value(value));
    json_object_object_get_ex(obj, "will_execute", &value);
    dst->will_execute = json_bool_value(value);
    json_object_object_get_ex(obj, "source", &value);
    dst->source = dup_str(json_string_value(value));
    json_object_object_get_ex(obj, "object", &value);
    dst->object = dup_str(json_string_value(value));
    json_object_object_get_ex(obj, "output", &value);
    dst->output = dup_str(json_string_value(value));
    json_object_object_get_ex(obj, "diagnostic_summary", &value);
    dst->diagnostic_summary = parse_summary(value);
    if (!dst->id || !dst->kind || !dst->status || !dst->source || !dst->object || !dst->output) {
        free_action(dst);
        return false;
    }
    return true;
}

static bool parse_snapshot(const char* graphPath, json_object* root, AnalysisBuildGraphSnapshot* out) {
    if (!graphPath || !root || !out || !json_object_is_type(root, json_type_object)) return false;
    memset(out, 0, sizeof(*out));
    json_object* value = NULL;
    json_object_object_get_ex(root, "schema", &value);
    const char* schema = json_string_value(value);
    if (strcmp(schema, "fisiCs.build_graph") != 0) return false;
    out->path = dup_str(graphPath);
    out->schema = dup_str(schema);
    json_object_object_get_ex(root, "version", &value);
    out->version = json_int_value(value);
    json_object_object_get_ex(root, "mode", &value);
    out->mode = dup_str(json_string_value(value));
    json_object_object_get_ex(root, "project_root", &value);
    out->project_root = dup_str(json_string_value(value));
    json_object_object_get_ex(root, "partial", &value);
    out->partial = json_bool_value(value);
    json_object_object_get_ex(root, "fatal", &value);
    out->fatal = json_bool_value(value);
    json_object_object_get_ex(root, "diagnostic_summary", &value);
    out->diagnostic_summary = parse_summary(value);
    if (!out->path || !out->schema || !out->mode || !out->project_root) {
        free_snapshot(out);
        return false;
    }

    json_object* arr = NULL;
    if (json_object_object_get_ex(root, "translation_units", &arr) && arr && json_object_is_type(arr, json_type_array)) {
        out->translation_unit_count = json_object_array_length(arr);
        if (out->translation_unit_count > 0) {
            out->translation_units = (AnalysisBuildGraphTranslationUnit*)calloc(out->translation_unit_count, sizeof(AnalysisBuildGraphTranslationUnit));
            if (!out->translation_units) {
                free_snapshot(out);
                return false;
            }
            for (size_t i = 0; i < out->translation_unit_count; ++i) {
                if (!clone_translation_unit(&out->translation_units[i], json_object_array_get_idx(arr, i))) {
                    free_snapshot(out);
                    return false;
                }
            }
        }
    }

    json_object* plan = NULL;
    json_object* actions = NULL;
    if (json_object_object_get_ex(root, "plan", &plan) && plan &&
        json_object_is_type(plan, json_type_object)) {
        json_object_object_get_ex(plan, "actions", &actions);
    }
    if ((!actions || !json_object_is_type(actions, json_type_array)) &&
        json_object_object_get_ex(root, "actions", &actions) &&
        actions && json_object_is_type(actions, json_type_array)) {
        /* Persisted summaries store the normalized action list at the top level. */
    }
    if (actions && json_object_is_type(actions, json_type_array)) {
        out->action_count = json_object_array_length(actions);
        if (out->action_count > 0) {
            out->actions = (AnalysisBuildGraphAction*)calloc(out->action_count, sizeof(AnalysisBuildGraphAction));
            if (!out->actions) {
                free_snapshot(out);
                return false;
            }
            for (size_t i = 0; i < out->action_count; ++i) {
                if (!clone_action(&out->actions[i], json_object_array_get_idx(actions, i))) {
                    free_snapshot(out);
                    return false;
                }
            }
        }
    }

    return true;
}

static bool upsert_snapshot(AnalysisBuildGraphSnapshot* snapshot) {
    if (!snapshot || !snapshot->path) return false;
    analysis_build_graph_store_lock();

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
        AnalysisBuildGraphSnapshot* grown = (AnalysisBuildGraphSnapshot*)realloc(g_snapshots, next * sizeof(AnalysisBuildGraphSnapshot));
        if (!grown) {
            analysis_build_graph_store_unlock();
            return false;
        }
        memset(grown + g_snapshot_cap, 0, (next - g_snapshot_cap) * sizeof(AnalysisBuildGraphSnapshot));
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
    analysis_build_graph_store_unlock();
    return true;
}

bool analysis_build_graph_store_load_json_text(const char* graphPath, const char* jsonText) {
    if (!graphPath || !*graphPath || !jsonText || !*jsonText) return false;
    json_object* root = json_tokener_parse(jsonText);
    if (!root) return false;
    AnalysisBuildGraphSnapshot snapshot = {0};
    bool ok = parse_snapshot(graphPath, root, &snapshot) && upsert_snapshot(&snapshot);
    free_snapshot(&snapshot);
    json_object_put(root);
    return ok;
}

bool analysis_build_graph_store_load_file(const char* graphPath) {
    if (!graphPath || !*graphPath) return false;
    FILE* f = fopen(graphPath, "r");
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
    bool ok = analysis_build_graph_store_load_json_text(graphPath, buf);
    free(buf);
    return ok;
}

void analysis_build_graph_store_remove(const char* graphPath) {
    if (!graphPath) return;
    analysis_build_graph_store_lock();
    size_t existing = (size_t)-1;
    for (size_t i = 0; i < g_snapshot_count; ++i) {
        if (g_snapshots[i].path && strcmp(g_snapshots[i].path, graphPath) == 0) {
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
    analysis_build_graph_store_unlock();
}

size_t analysis_build_graph_store_snapshot_count(void) {
    return g_snapshot_count;
}

const AnalysisBuildGraphSnapshot* analysis_build_graph_store_snapshot_at(size_t idx) {
    return idx < g_snapshot_count ? &g_snapshots[idx] : NULL;
}

uint64_t analysis_build_graph_store_combined_stamp(void) {
    analysis_build_graph_store_lock();
    uint64_t stamp = (uint64_t)g_snapshot_count;
    for (size_t i = 0; i < g_snapshot_count; ++i) {
        stamp ^= g_snapshots[i].stamp;
    }
    analysis_build_graph_store_unlock();
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

static json_object* summary_to_json(const AnalysisBuildGraphDiagnosticSummary* summary) {
    json_object* obj = json_object_new_object();
    json_object_object_add(obj, "available", json_object_new_boolean(summary ? summary->available : false));
    json_object_object_add(obj, "total", json_object_new_int(summary ? summary->total : 0));
    json_object_object_add(obj, "errors", json_object_new_int(summary ? summary->errors : 0));
    json_object_object_add(obj, "warnings", json_object_new_int(summary ? summary->warnings : 0));
    json_object_object_add(obj, "notes", json_object_new_int(summary ? summary->notes : 0));
    json_object_object_add(obj, "partial", json_object_new_boolean(summary ? summary->partial : false));
    json_object_object_add(obj, "fatal", json_object_new_boolean(summary ? summary->fatal : false));
    return obj;
}

static json_object* snapshot_to_json(const AnalysisBuildGraphSnapshot* snapshot) {
    json_object* obj = json_object_new_object();
    json_object_object_add(obj, "path", json_object_new_string(snapshot->path ? snapshot->path : ""));
    json_object_object_add(obj, "schema", json_object_new_string(snapshot->schema ? snapshot->schema : ""));
    json_object_object_add(obj, "version", json_object_new_int(snapshot->version));
    json_object_object_add(obj, "mode", json_object_new_string(snapshot->mode ? snapshot->mode : ""));
    json_object_object_add(obj, "project_root", json_object_new_string(snapshot->project_root ? snapshot->project_root : ""));
    json_object_object_add(obj, "partial", json_object_new_boolean(snapshot->partial));
    json_object_object_add(obj, "fatal", json_object_new_boolean(snapshot->fatal));
    json_object_object_add(obj, "diagnostic_summary", summary_to_json(&snapshot->diagnostic_summary));
    json_object_object_add(obj, "stamp", json_object_new_int64((long long)snapshot->stamp));

    json_object* tus = json_object_new_array();
    for (size_t i = 0; i < snapshot->translation_unit_count; ++i) {
        const AnalysisBuildGraphTranslationUnit* tu = &snapshot->translation_units[i];
        json_object* jt = json_object_new_object();
        json_object_object_add(jt, "id", json_object_new_string(tu->id ? tu->id : ""));
        json_object_object_add(jt, "source", json_object_new_string(tu->source ? tu->source : ""));
        json_object_object_add(jt, "object", json_object_new_string(tu->object ? tu->object : ""));
        json_object_object_add(jt, "status", json_object_new_string(tu->status ? tu->status : ""));
        json_object_object_add(jt, "diagnostic_summary", summary_to_json(&tu->diagnostic_summary));
        json_object_array_add(tus, jt);
    }
    json_object_object_add(obj, "translation_units", tus);

    json_object* actions = json_object_new_array();
    for (size_t i = 0; i < snapshot->action_count; ++i) {
        const AnalysisBuildGraphAction* action = &snapshot->actions[i];
        json_object* ja = json_object_new_object();
        json_object_object_add(ja, "id", json_object_new_string(action->id ? action->id : ""));
        json_object_object_add(ja, "kind", json_object_new_string(action->kind ? action->kind : ""));
        json_object_object_add(ja, "status", json_object_new_string(action->status ? action->status : ""));
        json_object_object_add(ja, "will_execute", json_object_new_boolean(action->will_execute));
        json_object_object_add(ja, "source", json_object_new_string(action->source ? action->source : ""));
        json_object_object_add(ja, "object", json_object_new_string(action->object ? action->object : ""));
        json_object_object_add(ja, "output", json_object_new_string(action->output ? action->output : ""));
        json_object_object_add(ja, "diagnostic_summary", summary_to_json(&action->diagnostic_summary));
        json_object_array_add(actions, ja);
    }
    json_object_object_add(obj, "actions", actions);
    return obj;
}

void analysis_build_graph_store_save(const char* workspaceRoot) {
    if (!workspaceRoot || !*workspaceRoot) return;
    ensure_cache_dir(workspaceRoot);
    char path[1024];
    snprintf(path, sizeof(path), "%s/ide_files/analysis_build_graph_summaries.json", workspaceRoot);

    analysis_build_graph_store_lock();
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
    analysis_build_graph_store_unlock();
}

void analysis_build_graph_store_load(const char* workspaceRoot) {
    analysis_build_graph_store_clear();
    if (!workspaceRoot || !*workspaceRoot) return;
    char path[1024];
    snprintf(path, sizeof(path), "%s/ide_files/analysis_build_graph_summaries.json", workspaceRoot);
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
        AnalysisBuildGraphSnapshot snapshot = {0};
        if (parse_snapshot(json_string_value(jpath), obj, &snapshot)) {
            json_object* jstamp = NULL;
            if (json_object_object_get_ex(obj, "stamp", &jstamp)) {
                long long stamp = json_object_get_int64(jstamp);
                if (stamp > 0) snapshot.stamp = (uint64_t)stamp;
            }
            upsert_snapshot(&snapshot);
        }
        free_snapshot(&snapshot);
    }
    json_object_put(root);
}
