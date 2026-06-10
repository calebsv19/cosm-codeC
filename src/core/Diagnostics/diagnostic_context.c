#include "core/Diagnostics/diagnostic_context.h"

#include <json-c/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static DiagnosticContextRecord* g_items = NULL;
static size_t g_count = 0;
static size_t g_cap = 0;

static char* dup_json(json_object* obj) {
    if (!obj) return NULL;
    const char* text = json_object_to_json_string_ext(obj, JSON_C_TO_STRING_PLAIN);
    return text ? strdup(text) : NULL;
}

static const char* obj_string(json_object* obj, const char* key) {
    json_object* value = NULL;
    if (!obj || !json_object_object_get_ex(obj, key, &value) ||
        !value || !json_object_is_type(value, json_type_string)) {
        return NULL;
    }
    return json_object_get_string(value);
}

static int obj_int(json_object* obj, const char* key) {
    json_object* value = NULL;
    if (!obj || !json_object_object_get_ex(obj, key, &value) ||
        !value || !json_object_is_type(value, json_type_int)) {
        return 0;
    }
    return json_object_get_int(value);
}

static void free_record(DiagnosticContextRecord* rec) {
    if (!rec) return;
    free(rec->file);
    free(rec->codeName);
    free(rec->message);
    free(rec->includeStackJson);
    free(rec->macroTraceJson);
    free(rec->detailsJson);
    memset(rec, 0, sizeof(*rec));
}

void diagnostic_context_clear(void) {
    for (size_t i = 0; i < g_count; ++i) {
        free_record(&g_items[i]);
    }
    free(g_items);
    g_items = NULL;
    g_count = 0;
    g_cap = 0;
}

static bool reserve_records(size_t extra) {
    if (extra == 0 || g_count + extra <= g_cap) return true;
    size_t need = g_count + extra;
    size_t next = g_cap ? g_cap * 2 : 8;
    while (next < need) next *= 2;
    DiagnosticContextRecord* grown =
        (DiagnosticContextRecord*)realloc(g_items, next * sizeof(DiagnosticContextRecord));
    if (!grown) return false;
    memset(grown + g_cap, 0, (next - g_cap) * sizeof(DiagnosticContextRecord));
    g_items = grown;
    g_cap = next;
    return true;
}

static bool append_record_from_diag(json_object* diag) {
    json_object* include_stack = NULL;
    json_object* macro_trace = NULL;
    json_object* details = NULL;
    bool has_include = json_object_object_get_ex(diag, "include_stack", &include_stack) &&
                       include_stack && json_object_is_type(include_stack, json_type_array);
    bool has_macro = json_object_object_get_ex(diag, "macro_trace", &macro_trace) &&
                     macro_trace && json_object_is_type(macro_trace, json_type_array);
    bool has_details = json_object_object_get_ex(diag, "details", &details) &&
                       details && json_object_is_type(details, json_type_object);
    if (!has_include && !has_macro && !has_details) return true;
    if (!reserve_records(1)) return false;

    DiagnosticContextRecord rec = {0};
    const char* file = obj_string(diag, "file");
    const char* code_name = obj_string(diag, "code_name");
    const char* message = obj_string(diag, "message");
    rec.file = file && file[0] ? strdup(file) : NULL;
    rec.line = obj_int(diag, "line");
    rec.column = obj_int(diag, "column");
    if (rec.column == 0) rec.column = obj_int(diag, "col");
    rec.length = obj_int(diag, "length");
    rec.codeId = obj_int(diag, "code_id");
    rec.codeName = code_name && code_name[0] ? strdup(code_name) : NULL;
    rec.message = message && message[0] ? strdup(message) : NULL;
    rec.includeStackJson = has_include ? dup_json(include_stack) : NULL;
    rec.macroTraceJson = has_macro ? dup_json(macro_trace) : NULL;
    rec.detailsJson = has_details ? dup_json(details) : NULL;

    if ((file && file[0] && !rec.file) ||
        (code_name && code_name[0] && !rec.codeName) ||
        (message && message[0] && !rec.message) ||
        (has_include && !rec.includeStackJson) ||
        (has_macro && !rec.macroTraceJson) ||
        (has_details && !rec.detailsJson)) {
        free_record(&rec);
        return false;
    }

    g_items[g_count++] = rec;
    return true;
}

bool diagnostic_context_load_json_text(const char* text) {
    if (!text || !*text) return false;
    json_object* root = json_tokener_parse(text);
    if (!root) return false;

    json_object* diagnostics = root;
    if (json_object_is_type(root, json_type_object)) {
        json_object_object_get_ex(root, "diagnostics", &diagnostics);
    }
    if (!diagnostics || !json_object_is_type(diagnostics, json_type_array)) {
        json_object_put(root);
        return false;
    }

    diagnostic_context_clear();
    size_t count = json_object_array_length(diagnostics);
    for (size_t i = 0; i < count; ++i) {
        json_object* diag = json_object_array_get_idx(diagnostics, i);
        if (!diag || !json_object_is_type(diag, json_type_object)) continue;
        if (!append_record_from_diag(diag)) {
            json_object_put(root);
            diagnostic_context_clear();
            return false;
        }
    }
    json_object_put(root);
    return true;
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

bool diagnostic_context_save(const char* workspaceRoot) {
    if (!workspaceRoot || !*workspaceRoot) return false;
    ensure_cache_dir(workspaceRoot);
    char path[1024];
    snprintf(path, sizeof(path), "%s/ide_files/diagnostic_context.json", workspaceRoot);

    json_object* root = json_object_new_object();
    json_object* arr = json_object_new_array();
    if (!root || !arr) {
        if (root) json_object_put(root);
        if (arr) json_object_put(arr);
        return false;
    }
    json_object_object_add(root, "profile", json_object_new_string("ide_diagnostic_context_v1"));
    json_object_object_add(root, "schema_version", json_object_new_int(1));
    for (size_t i = 0; i < g_count; ++i) {
        const DiagnosticContextRecord* rec = &g_items[i];
        json_object* obj = json_object_new_object();
        json_object_object_add(obj, "file", json_object_new_string(rec->file ? rec->file : ""));
        json_object_object_add(obj, "line", json_object_new_int(rec->line));
        json_object_object_add(obj, "column", json_object_new_int(rec->column));
        json_object_object_add(obj, "length", json_object_new_int(rec->length));
        json_object_object_add(obj, "code_id", json_object_new_int(rec->codeId));
        json_object_object_add(obj, "code_name", json_object_new_string(rec->codeName ? rec->codeName : ""));
        json_object_object_add(obj, "message", json_object_new_string(rec->message ? rec->message : ""));
        if (rec->includeStackJson) {
            json_object* parsed = json_tokener_parse(rec->includeStackJson);
            if (parsed) json_object_object_add(obj, "include_stack", parsed);
        }
        if (rec->macroTraceJson) {
            json_object* parsed = json_tokener_parse(rec->macroTraceJson);
            if (parsed) json_object_object_add(obj, "macro_trace", parsed);
        }
        if (rec->detailsJson) {
            json_object* parsed = json_tokener_parse(rec->detailsJson);
            if (parsed) json_object_object_add(obj, "details", parsed);
        }
        json_object_array_add(arr, obj);
    }
    json_object_object_add(root, "diagnostics", arr);

    const char* serialized = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
    FILE* f = fopen(path, "w");
    bool ok = false;
    if (f && serialized) {
        ok = fputs(serialized, f) >= 0;
    }
    if (f) fclose(f);
    json_object_put(root);
    return ok;
}

bool diagnostic_context_load(const char* workspaceRoot) {
    if (!workspaceRoot || !*workspaceRoot) return false;
    char path[1024];
    snprintf(path, sizeof(path), "%s/ide_files/diagnostic_context.json", workspaceRoot);
    FILE* f = fopen(path, "r");
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
    bool ok = diagnostic_context_load_json_text(buf);
    free(buf);
    return ok;
}

size_t diagnostic_context_count(void) {
    return g_count;
}

const DiagnosticContextRecord* diagnostic_context_at(size_t index) {
    return index < g_count ? &g_items[index] : NULL;
}

static bool text_matches(const char* expected, const char* actual) {
    return !expected || !expected[0] || (actual && strcmp(expected, actual) == 0);
}

const DiagnosticContextRecord* diagnostic_context_find(const char* file,
                                                       int line,
                                                       int column,
                                                       int codeId,
                                                       const char* codeName,
                                                       const char* message) {
    for (size_t i = 0; i < g_count; ++i) {
        const DiagnosticContextRecord* rec = &g_items[i];
        if (file && file[0] && rec->file && strcmp(file, rec->file) != 0) continue;
        if (line > 0 && rec->line > 0 && line != rec->line) continue;
        if (column > 0 && rec->column > 0 && column != rec->column) continue;
        if (codeId > 0 && rec->codeId > 0 && codeId != rec->codeId) continue;
        if (!text_matches(codeName, rec->codeName)) continue;
        if (!text_matches(message, rec->message)) continue;
        return rec;
    }
    return NULL;
}
