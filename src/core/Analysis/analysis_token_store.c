#include "core/Analysis/analysis_token_store.h"

#include <json-c/json.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "core/Analysis/analysis_artifact_io.h"

static AnalysisFileTokens* g_files = NULL;
static size_t g_file_count = 0;
static size_t g_file_cap = 0;
static uint64_t g_stamp_counter = 0;

static void free_token_entry(AnalysisFileTokens* f) {
    if (!f) return;
    free(f->path);
    free(f->spans);
    f->path = NULL;
    f->spans = NULL;
    f->count = 0;
    f->stamp = 0;
}

void analysis_token_store_clear(void) {
    for (size_t i = 0; i < g_file_count; ++i) {
        free_token_entry(&g_files[i]);
    }
    free(g_files);
    g_files = NULL;
    g_file_count = 0;
    g_file_cap = 0;
    g_stamp_counter = 0;
}

static char* dup_str(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char* out = malloc(len);
    if (!out) return NULL;
    memcpy(out, s, len);
    return out;
}

void analysis_token_store_upsert(const char* filePath,
                                 const FisicsTokenSpan* spans,
                                 size_t spanCount) {
    if (!filePath) return;

    size_t existing = (size_t)-1;
    for (size_t i = 0; i < g_file_count; ++i) {
        if (g_files[i].path && strcmp(g_files[i].path, filePath) == 0) {
            existing = i;
            break;
        }
    }
    if (existing != (size_t)-1) {
        free_token_entry(&g_files[existing]);
        for (size_t j = existing + 1; j < g_file_count; ++j) {
            g_files[j - 1] = g_files[j];
        }
        g_file_count--;
    }

    if (g_file_count >= g_file_cap) {
        size_t newCap = g_file_cap ? g_file_cap * 2 : 8;
        AnalysisFileTokens* tmp = realloc(g_files, newCap * sizeof(AnalysisFileTokens));
        if (!tmp) return;
        g_files = tmp;
        g_file_cap = newCap;
    }

    AnalysisFileTokens entry = {0};
    entry.path = dup_str(filePath);
    entry.count = spanCount;
    entry.stamp = ++g_stamp_counter;

    if (spanCount > 0 && spans) {
        entry.spans = (FisicsTokenSpan*)malloc(spanCount * sizeof(FisicsTokenSpan));
        if (!entry.spans) {
            free(entry.path);
            return;
        }
        memcpy(entry.spans, spans, spanCount * sizeof(FisicsTokenSpan));
    }

    for (size_t j = g_file_count; j > 0; --j) {
        g_files[j] = g_files[j - 1];
    }
    g_files[0] = entry;
    g_file_count++;
}

void analysis_token_store_remove(const char* filePath) {
    if (!filePath) return;
    size_t existing = (size_t)-1;
    for (size_t i = 0; i < g_file_count; ++i) {
        if (g_files[i].path && strcmp(g_files[i].path, filePath) == 0) {
            existing = i;
            break;
        }
    }
    if (existing == (size_t)-1) return;
    free_token_entry(&g_files[existing]);
    for (size_t j = existing + 1; j < g_file_count; ++j) {
        g_files[j - 1] = g_files[j];
    }
    g_file_count--;
}

size_t analysis_token_store_file_count(void) {
    return g_file_count;
}

const AnalysisFileTokens* analysis_token_store_file_at(size_t idx) {
    if (idx >= g_file_count) return NULL;
    return &g_files[idx];
}

void analysis_token_store_save(const char* workspaceRoot) {
    if (!workspaceRoot || !*workspaceRoot) return;
    if (!analysis_artifact_io_ensure_dir(workspaceRoot)) return;
    char path[1024];
    char tmpPath[1024];
    if (!analysis_artifact_io_path(workspaceRoot, "analysis_tokens.json", path, sizeof(path))) return;
    snprintf(tmpPath, sizeof(tmpPath), "%s.tmp", path);

    FILE* out = fopen(tmpPath, "w");
    if (!out) return;
    size_t written = 0;
    bool ok = true;
#define WRITE_LIMITED_LITERAL(lit) \
    do { \
        const char* _s = (lit); \
        size_t _n = strlen(_s); \
        if (written + _n > ANALYSIS_TOKEN_STORE_PERSIST_LIMIT_BYTES || \
            fwrite(_s, 1, _n, out) != _n) { \
            ok = false; \
        } else { \
            written += _n; \
        } \
    } while (0)
#define WRITE_LIMITED_FORMAT(fmt, ...) \
    do { \
        char _buf[128]; \
        int _n = snprintf(_buf, sizeof(_buf), (fmt), __VA_ARGS__); \
        if (_n < 0 || (size_t)_n >= sizeof(_buf) || \
            written + (size_t)_n > ANALYSIS_TOKEN_STORE_PERSIST_LIMIT_BYTES || \
            fwrite(_buf, 1, (size_t)_n, out) != (size_t)_n) { \
            ok = false; \
        } else { \
            written += (size_t)_n; \
        } \
    } while (0)

    WRITE_LIMITED_LITERAL("[");
    for (size_t i = 0; i < g_file_count; ++i) {
        if (!ok) break;
        AnalysisFileTokens* f = &g_files[i];
        if (i > 0) WRITE_LIMITED_LITERAL(",");
        WRITE_LIMITED_LITERAL("{\"path\":");
        if (!ok) break;
        json_object* escaped_path = json_object_new_string(f->path ? f->path : "");
        const char* path_json = escaped_path
                                    ? json_object_to_json_string_ext(escaped_path, JSON_C_TO_STRING_PLAIN)
                                    : "\"\"";
        WRITE_LIMITED_LITERAL(path_json ? path_json : "\"\"");
        if (escaped_path) json_object_put(escaped_path);
        WRITE_LIMITED_FORMAT(",\"stamp\":%llu,\"spans\":[",
                             (unsigned long long)f->stamp);
        for (size_t s = 0; s < f->count; ++s) {
            if (!ok) break;
            const FisicsTokenSpan* span = &f->spans[s];
            if (s > 0) WRITE_LIMITED_LITERAL(",");
            WRITE_LIMITED_FORMAT("{\"line\":%d,\"column\":%d,\"length\":%d,\"kind\":%d}",
                                 span->line,
                                 span->column,
                                 span->length,
                                 (int)span->kind);
        }
        WRITE_LIMITED_LITERAL("]}");
    }
    WRITE_LIMITED_LITERAL("]");

#undef WRITE_LIMITED_FORMAT
#undef WRITE_LIMITED_LITERAL

    if (fclose(out) != 0) {
        ok = false;
    }

    if (!ok) {
        unlink(tmpPath);
        unlink(path);
        return;
    }
    if (rename(tmpPath, path) != 0) {
        unlink(tmpPath);
    }
}

void analysis_token_store_load(const char* workspaceRoot) {
    analysis_token_store_clear();
    if (!workspaceRoot || !*workspaceRoot) return;
    char path[1024];
    if (!analysis_artifact_io_path(workspaceRoot, "analysis_tokens.json", path, sizeof(path))) return;
    FILE* f = fopen(path, "r");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0) {
        fclose(f);
        return;
    }
    if (len > (long)ANALYSIS_TOKEN_STORE_PERSIST_LIMIT_BYTES) {
        fclose(f);
        unlink(path);
        return;
    }
    char* buf = malloc((size_t)len + 1);
    if (!buf) {
        fclose(f);
        return;
    }
    fread(buf, 1, (size_t)len, f);
    buf[len] = '\0';
    fclose(f);

    json_object* root = json_tokener_parse(buf);
    free(buf);
    if (!root || !json_object_is_type(root, json_type_array)) {
        if (root) json_object_put(root);
        return;
    }

    size_t arrLen = json_object_array_length(root);
    for (size_t i = 0; i < arrLen; ++i) {
        json_object* obj = json_object_array_get_idx(root, i);
        if (!obj) continue;
        json_object* jpath = NULL;
        json_object* jstamp = NULL;
        json_object* jspans = NULL;
        if (!json_object_object_get_ex(obj, "path", &jpath) ||
            !json_object_object_get_ex(obj, "spans", &jspans)) {
            continue;
        }
        const char* pathStr = json_object_get_string(jpath);
        size_t scount = json_object_array_length(jspans);
        FisicsTokenSpan* tmp = calloc(scount, sizeof(FisicsTokenSpan));
        if (!tmp) continue;
        for (size_t s = 0; s < scount; ++s) {
            json_object* js = json_object_array_get_idx(jspans, s);
            if (!js) continue;
            json_object* jl = NULL;
            json_object* jc = NULL;
            json_object* jlen = NULL;
            json_object* jk = NULL;
            json_object_object_get_ex(js, "line", &jl);
            json_object_object_get_ex(js, "column", &jc);
            json_object_object_get_ex(js, "length", &jlen);
            json_object_object_get_ex(js, "kind", &jk);
            tmp[s].line = jl ? json_object_get_int(jl) : 0;
            tmp[s].column = jc ? json_object_get_int(jc) : 0;
            tmp[s].length = jlen ? json_object_get_int(jlen) : 0;
            tmp[s].kind = jk ? (FisicsTokenKind)json_object_get_int(jk) : FISICS_TOK_IDENTIFIER;
        }
        analysis_token_store_upsert(pathStr, tmp, scount);
        free(tmp);
        if (json_object_object_get_ex(obj, "stamp", &jstamp)) {
            long long s = json_object_get_int64(jstamp);
            if (s > 0 && (uint64_t)s > g_stamp_counter) g_stamp_counter = (uint64_t)s;
        }
    }
    json_object_put(root);
}
