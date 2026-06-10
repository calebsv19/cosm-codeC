#include "core/Diagnostics/diagnostic_explanations.h"

#include <json-c/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "Compiler/diagnostic_metadata.h"

static DiagnosticExplanation* g_items = NULL;
static size_t g_count = 0;
static size_t g_cap = 0;

static char* dup_or_empty(const char* text) {
    return strdup(text ? text : "");
}

static void free_item(DiagnosticExplanation* item) {
    if (!item) return;
    free(item->codeName);
    free(item->categoryName);
    free(item->stage);
    free(item->description);
    free(item->commonCauses);
    free(item->nextAction);
    memset(item, 0, sizeof(*item));
}

void diagnostic_explanations_clear(void) {
    for (size_t i = 0; i < g_count; ++i) {
        free_item(&g_items[i]);
    }
    free(g_items);
    g_items = NULL;
    g_count = 0;
    g_cap = 0;
}

static bool ensure_capacity(size_t needed) {
    if (needed <= g_cap) return true;
    size_t newCap = g_cap ? g_cap * 2u : 16u;
    while (newCap < needed) newCap *= 2u;
    DiagnosticExplanation* tmp = realloc(g_items, newCap * sizeof(DiagnosticExplanation));
    if (!tmp) return false;
    g_items = tmp;
    g_cap = newCap;
    return true;
}

static bool append_item(int codeId,
                        const char* codeName,
                        int categoryId,
                        const char* categoryName,
                        const char* stage,
                        const char* description,
                        const char* commonCauses,
                        const char* nextAction) {
    if (!ensure_capacity(g_count + 1u)) return false;
    DiagnosticExplanation item;
    memset(&item, 0, sizeof(item));
    item.codeId = codeId;
    item.codeName = dup_or_empty(codeName);
    item.categoryId = categoryId;
    item.categoryName = dup_or_empty(categoryName);
    item.stage = dup_or_empty(stage);
    item.description = dup_or_empty(description);
    item.commonCauses = dup_or_empty(commonCauses);
    item.nextAction = dup_or_empty(nextAction);
    if (!item.codeName || !item.categoryName || !item.stage ||
        !item.description || !item.commonCauses || !item.nextAction) {
        free_item(&item);
        return false;
    }
    g_items[g_count++] = item;
    return true;
}

bool diagnostic_explanations_refresh_from_fisics_metadata(void) {
    size_t count = 0;
    const FisicsDiagnosticExplanation* explanations = fisics_diag_explanations(&count);
    diagnostic_explanations_clear();
    for (size_t i = 0; i < count; ++i) {
        int codeId = explanations[i].code_id;
        int categoryId = fisics_diag_category_id_from_code(codeId);
        if (!append_item(codeId,
                         fisics_diag_code_name(codeId),
                         categoryId,
                         fisics_diag_category_name(categoryId),
                         fisics_diag_stage_name_from_code(codeId),
                         explanations[i].description,
                         explanations[i].common_causes,
                         explanations[i].next_action)) {
            diagnostic_explanations_clear();
            return false;
        }
    }
    return true;
}

static const char* json_string_or_empty(json_object* obj, const char* key) {
    json_object* value = NULL;
    if (!obj || !json_object_object_get_ex(obj, key, &value) || !value) return "";
    const char* text = json_object_get_string(value);
    return text ? text : "";
}

static int json_int_or_zero(json_object* obj, const char* key) {
    json_object* value = NULL;
    if (!obj || !json_object_object_get_ex(obj, key, &value) || !value) return 0;
    return json_object_get_int(value);
}

bool diagnostic_explanations_load_json_text(const char* jsonText) {
    if (!jsonText || !*jsonText) return false;
    json_object* root = json_tokener_parse(jsonText);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        return false;
    }
    json_object* diagnostics = NULL;
    if (!json_object_object_get_ex(root, "diagnostics", &diagnostics) ||
        !diagnostics ||
        !json_object_is_type(diagnostics, json_type_array)) {
        json_object_put(root);
        return false;
    }

    diagnostic_explanations_clear();
    size_t count = json_object_array_length(diagnostics);
    for (size_t i = 0; i < count; ++i) {
        json_object* item = json_object_array_get_idx(diagnostics, i);
        if (!item || !json_object_is_type(item, json_type_object)) continue;
        int codeId = json_int_or_zero(item, "code_id");
        if (!append_item(codeId,
                         json_string_or_empty(item, "code_name"),
                         json_int_or_zero(item, "category_id"),
                         json_string_or_empty(item, "category_name"),
                         json_string_or_empty(item, "stage"),
                         json_string_or_empty(item, "description"),
                         json_string_or_empty(item, "common_causes"),
                         json_string_or_empty(item, "next_action"))) {
            json_object_put(root);
            diagnostic_explanations_clear();
            return false;
        }
    }
    json_object_put(root);
    return true;
}

static bool ensure_cache_dir(const char* workspaceRoot) {
    if (!workspaceRoot || !*workspaceRoot) return false;
    char dir[1024];
    snprintf(dir, sizeof(dir), "%s/ide_files", workspaceRoot);
    struct stat st;
    if (stat(dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
        if (mkdir(dir, 0755) != 0) return false;
    }
    return true;
}

bool diagnostic_explanations_save(const char* workspaceRoot) {
    if (!ensure_cache_dir(workspaceRoot)) return false;
    char path[1024];
    snprintf(path, sizeof(path), "%s/ide_files/diagnostic_explanations.json", workspaceRoot);
    json_object* root = json_object_new_object();
    json_object_object_add(root, "profile", json_object_new_string("fisics_diagnostic_explanations"));
    json_object_object_add(root, "schema_version", json_object_new_int(1));
    json_object* diagnostics = json_object_new_array();
    for (size_t i = 0; i < g_count; ++i) {
        const DiagnosticExplanation* item = &g_items[i];
        json_object* obj = json_object_new_object();
        json_object_object_add(obj, "code_id", json_object_new_int(item->codeId));
        json_object_object_add(obj, "code_name", json_object_new_string(item->codeName ? item->codeName : ""));
        json_object_object_add(obj, "category_id", json_object_new_int(item->categoryId));
        json_object_object_add(obj, "category_name", json_object_new_string(item->categoryName ? item->categoryName : ""));
        json_object_object_add(obj, "stage", json_object_new_string(item->stage ? item->stage : ""));
        json_object_object_add(obj, "description", json_object_new_string(item->description ? item->description : ""));
        json_object_object_add(obj, "common_causes", json_object_new_string(item->commonCauses ? item->commonCauses : ""));
        json_object_object_add(obj, "next_action", json_object_new_string(item->nextAction ? item->nextAction : ""));
        json_object_array_add(diagnostics, obj);
    }
    json_object_object_add(root, "diagnostics", diagnostics);

    const char* serialized = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
    FILE* f = fopen(path, "w");
    if (!f || !serialized) {
        if (f) fclose(f);
        json_object_put(root);
        return false;
    }
    fputs(serialized, f);
    fclose(f);
    json_object_put(root);
    return true;
}

bool diagnostic_explanations_load(const char* workspaceRoot) {
    if (!workspaceRoot || !*workspaceRoot) return false;
    char path[1024];
    snprintf(path, sizeof(path), "%s/ide_files/diagnostic_explanations.json", workspaceRoot);
    FILE* f = fopen(path, "r");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0 || len > (4 * 1024 * 1024)) {
        fclose(f);
        return false;
    }
    char* buf = malloc((size_t)len + 1u);
    if (!buf) {
        fclose(f);
        return false;
    }
    size_t readLen = fread(buf, 1, (size_t)len, f);
    fclose(f);
    buf[readLen] = '\0';
    bool ok = diagnostic_explanations_load_json_text(buf);
    free(buf);
    return ok;
}

size_t diagnostic_explanations_count(void) {
    return g_count;
}

const DiagnosticExplanation* diagnostic_explanations_at(size_t index) {
    if (index >= g_count) return NULL;
    return &g_items[index];
}

const DiagnosticExplanation* diagnostic_explanations_find_by_code(int codeId) {
    if (g_count == 0) {
        diagnostic_explanations_refresh_from_fisics_metadata();
    }
    for (size_t i = 0; i < g_count; ++i) {
        if (g_items[i].codeId == codeId) return &g_items[i];
    }
    return NULL;
}

const DiagnosticExplanation* diagnostic_explanations_find_by_name(const char* codeName) {
    if (!codeName || !*codeName) return NULL;
    if (g_count == 0) {
        diagnostic_explanations_refresh_from_fisics_metadata();
    }
    for (size_t i = 0; i < g_count; ++i) {
        if (g_items[i].codeName && strcmp(g_items[i].codeName, codeName) == 0) {
            return &g_items[i];
        }
    }
    return NULL;
}
